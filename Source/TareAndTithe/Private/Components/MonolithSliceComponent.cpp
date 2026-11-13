// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/MonolithSliceComponent.h"
#include "UDynamicMesh.h"
#include "Components/DynamicMeshComponent.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "DynamicSubmesh3.h"
#include "DynamicMesh/DynamicMeshAABBTree3.h"
#include "DynamicMesh/MeshTransforms.h"
#include "DynamicMesh/Operations/MergeCoincidentMeshEdges.h"
#include "DynamicMeshEditor.h"
#include "Operations/MeshPlaneCut.h"
#include "Selections/MeshConnectedComponents.h"
#include "Spatial/FastWinding.h"
#include "ConstrainedDelaunay2.h"
#include "MeshQueries.h"
#include "GeometryScript/MeshPrimitiveFunctions.h"
#include "GeometryScript/MeshAssetFunctions.h"
#include "GeometryScript/MeshUVFunctions.h"
#include "GeometryScript/CollisionFunctions.h"

#include "TareAndTithe.h"
#include "Actors/SeveredChunkActor.h"
#include "Components/VeinScannerComponent.h"
#include "Data/MonolithContractDataAsset.h"
#include "Data/ShiftProgressionDataAsset.h"
#include "Subsystems/ChunkPoolSubsystem.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInterface.h"

namespace
{
	constexpr double SliceVertexSignEpsilon = 1.0e-3;
	constexpr double CoincidentVertexMergeTolerance = 0.001;
	constexpr double FastWindingIsoThreshold = 0.5;
	constexpr int32 MinValidIslandTriangles = 4;
	constexpr int32 MaxProceduralTriangleBudget = 3500;

	struct FExtractedSliceIsland
	{
		UE::Geometry::FDynamicMesh3 Mesh;
		UE::Geometry::FAxisAlignedBox3d LocalBounds = UE::Geometry::FAxisAlignedBox3d::Empty();
		FVector3d LocalCoMCm = FVector3d::Zero();
		double VolumeCm3 = 0.0;
		bool bIsGripped = false;
		bool bIsContaminated = false;
	};

	// In UE5's left-handed FDynamicMesh3 convention (matching TMeshQueries::GetVolumeArea where N = (V2 - V0).Cross(V1 - V0)),
	// 6 * SignedTetrahedronVolume(0, A, B, C) = A.Dot(C.Cross(B)) = -A.Dot(B.Cross(C)).
	inline double SignedTetSixVolumeUE(const FVector3d& A, const FVector3d& B, const FVector3d& C)
	{
		return A.Dot(C.Cross(B));
	}

	bool DoesSegment2DOverlapRect(
		const FVector2d& A,
		const FVector2d& B,
		double HalfU,
		double HalfV)
	{
		double TMin = 0.0;
		double TMax = 1.0;
		const double Du = B.X - A.X;
		const double Dv = B.Y - A.Y;

		auto ClipAxis = [&TMin, &TMax](double P, double Q) -> bool
		{
			if (FMath::Abs(P) <= 1.0e-12)
			{
				return Q >= -1.0e-6;
			}
			const double R = Q / P;
			if (P < 0.0)
			{
				if (R > TMax)
				{
					return false;
				}
				if (R > TMin)
				{
					TMin = R;
				}
			}
			else
			{
				if (R < TMin)
				{
					return false;
				}
				if (R < TMax)
				{
					TMax = R;
				}
			}
			return TMin <= TMax;
		};

		return ClipAxis(-Du, A.X + HalfU) &&
		       ClipAxis(+Du, HalfU - A.X) &&
		       ClipAxis(-Dv, A.Y + HalfV) &&
		       ClipAxis(+Dv, HalfV - A.Y);
	}

	bool IsOrigin2DInsideTriangle(const FVector2d& A, const FVector2d& B, const FVector2d& C)
	{
		const double C0 = (B.X - A.X) * (-A.Y) - (B.Y - A.Y) * (-A.X);
		const double C1 = (C.X - B.X) * (-B.Y) - (C.Y - B.Y) * (-B.X);
		const double C2 = (A.X - C.X) * (-C.Y) - (A.Y - C.Y) * (-C.X);
		const bool bHasNeg = (C0 < -1.0e-6) || (C1 < -1.0e-6) || (C2 < -1.0e-6);
		const bool bHasPos = (C0 > 1.0e-6) || (C1 > 1.0e-6) || (C2 > 1.0e-6);
		return !(bHasNeg && bHasPos);
	}

	bool DoesPlaneAndApertureIntersectMesh(
		const UE::Geometry::FDynamicMesh3& SourceMesh,
		const FTransform& RemainderTransform,
		const FVector& PlaneOriginWorld,
		const FVector3d& LocalOrigin,
		const FVector3d& LocalNormal,
		const FLaserSliceAperture& SheetAperture)
	{
		double MinSignedDist = TNumericLimits<double>::Max();
		double MaxSignedDist = TNumericLimits<double>::Lowest();

		for (int32 Vid : SourceMesh.VertexIndicesItr())
		{
			const double SignedDist = (SourceMesh.GetVertex(Vid) - LocalOrigin).Dot(LocalNormal);
			MinSignedDist = FMath::Min(MinSignedDist, SignedDist);
			MaxSignedDist = FMath::Max(MaxSignedDist, SignedDist);
		}

		if (!(MinSignedDist < -SliceVertexSignEpsilon && MaxSignedDist > SliceVertexSignEpsilon))
		{
			return false;
		}

		if (!SheetAperture.bEnforceFiniteSheetBounds)
		{
			return true;
		}

		const FVector3d AxisU = static_cast<FVector3d>(SheetAperture.SheetForwardWorld).GetSafeNormal();
		const FVector3d AxisV = static_cast<FVector3d>(SheetAperture.SheetRightWorld).GetSafeNormal();
		if (AxisU.IsNearlyZero() || AxisV.IsNearlyZero())
		{
			return true;
		}

		const FVector3d OriginW = static_cast<FVector3d>(PlaneOriginWorld);
		const double HalfU = FMath::Max(1.0, static_cast<double>(SheetAperture.HalfDepthCm));
		const double HalfV = FMath::Max(1.0, static_cast<double>(SheetAperture.HalfWidthCm));
		constexpr double TriPlaneEps = 1.0e-5;

		auto ProjectLocalPointToSheet2D = [&RemainderTransform, &OriginW, &AxisU, &AxisV](const FVector3d& LocalPt) -> FVector2d
		{
			const FVector3d WorldPt = static_cast<FVector3d>(RemainderTransform.TransformPosition(static_cast<FVector>(LocalPt)));
			const FVector3d RelW = WorldPt - OriginW;
			return FVector2d(RelW.Dot(AxisU), RelW.Dot(AxisV));
		};

		for (int32 Tid : SourceMesh.TriangleIndicesItr())
		{
			FVector3d V[3];
			SourceMesh.GetTriVertices(Tid, V[0], V[1], V[2]);

			const double D[3] = {
				(V[0] - LocalOrigin).Dot(LocalNormal),
				(V[1] - LocalOrigin).Dot(LocalNormal),
				(V[2] - LocalOrigin).Dot(LocalNormal)
			};

			if ((D[0] > TriPlaneEps && D[1] > TriPlaneEps && D[2] > TriPlaneEps) ||
				(D[0] < -TriPlaneEps && D[1] < -TriPlaneEps && D[2] < -TriPlaneEps))
			{
				continue;
			}

			FVector3d HitLocal[3];
			int32 HitCount = 0;

			for (int32 m = 0; m < 3; ++m)
			{
				if (FMath::Abs(D[m]) <= TriPlaneEps)
				{
					HitLocal[HitCount++] = V[m];
				}
			}

			for (int32 EdgeIdx = 0; EdgeIdx < 3 && HitCount < 3; ++EdgeIdx)
			{
				const int32 AIdx = EdgeIdx;
				const int32 BIdx = (EdgeIdx + 1) % 3;
				if ((D[AIdx] < -TriPlaneEps && D[BIdx] > TriPlaneEps) ||
					(D[AIdx] > TriPlaneEps && D[BIdx] < -TriPlaneEps))
				{
					const double Alpha = D[AIdx] / (D[AIdx] - D[BIdx]);
					HitLocal[HitCount++] = V[AIdx] + Alpha * (V[BIdx] - V[AIdx]);
				}
			}

			if (HitCount == 1)
			{
				const FVector2d Pt2D = ProjectLocalPointToSheet2D(HitLocal[0]);
				if (DoesSegment2DOverlapRect(Pt2D, Pt2D, HalfU, HalfV))
				{
					return true;
				}
			}
			else if (HitCount == 2)
			{
				const FVector2d SegA = ProjectLocalPointToSheet2D(HitLocal[0]);
				const FVector2d SegB = ProjectLocalPointToSheet2D(HitLocal[1]);
				if (DoesSegment2DOverlapRect(SegA, SegB, HalfU, HalfV))
				{
					return true;
				}
			}
			else if (HitCount == 3)
			{
				const FVector2d P0 = ProjectLocalPointToSheet2D(HitLocal[0]);
				const FVector2d P1 = ProjectLocalPointToSheet2D(HitLocal[1]);
				const FVector2d P2 = ProjectLocalPointToSheet2D(HitLocal[2]);
				if (DoesSegment2DOverlapRect(P0, P1, HalfU, HalfV) ||
					DoesSegment2DOverlapRect(P1, P2, HalfU, HalfV) ||
					DoesSegment2DOverlapRect(P2, P0, HalfU, HalfV) ||
					IsOrigin2DInsideTriangle(P0, P1, P2))
				{
					return true;
				}
			}
		}

		return false;
	}

	UE::Geometry::FAxisAlignedBox3d ComputeTopGripLocalBounds3d(
		const UE::Geometry::FDynamicMesh3& Mesh,
		const UE::Geometry::FAxisAlignedBox3d& FullBounds,
		double RequestedSampleDepthCm)
	{
		if (FullBounds.IsEmpty())
		{
			return FullBounds;
		}

		const double FullHeightZ = FMath::Max(1.0, FullBounds.Max.Z - FullBounds.Min.Z);
		const double SampleDepth = FMath::Clamp(RequestedSampleDepthCm, 2.0, 0.45 * FullHeightZ);
		const double ZPlaneLow = FullBounds.Max.Z - SampleDepth;
		const double ZPlaneMid = FullBounds.Max.Z - (0.5 * SampleDepth);

		UE::Geometry::FAxisAlignedBox3d TopBandBounds = UE::Geometry::FAxisAlignedBox3d::Empty();

		for (int32 Vid : Mesh.VertexIndicesItr())
		{
			const FVector3d Pos = Mesh.GetVertex(Vid);
			if (Pos.Z >= ZPlaneLow - 1.0e-4)
			{
				TopBandBounds.Contain(Pos);
			}
		}

		auto AccumulateEdgePlaneCrossing = [&TopBandBounds](const FVector3d& A, const FVector3d& B, double PlaneZ)
		{
			const double Da = A.Z - PlaneZ;
			const double Db = B.Z - PlaneZ;
			if ((Da <= 0.0 && Db >= 0.0) || (Da >= 0.0 && Db <= 0.0))
			{
				const double Denom = B.Z - A.Z;
				if (FMath::Abs(Denom) > 1.0e-8)
				{
					const double Alpha = FMath::Clamp((PlaneZ - A.Z) / Denom, 0.0, 1.0);
					TopBandBounds.Contain(A + Alpha * (B - A));
				}
			}
		};

		for (int32 Tid : Mesh.TriangleIndicesItr())
		{
			FVector3d TriV[3];
			Mesh.GetTriVertices(Tid, TriV[0], TriV[1], TriV[2]);
			const double TriMinZ = FMath::Min3(TriV[0].Z, TriV[1].Z, TriV[2].Z);
			const double TriMaxZ = FMath::Max3(TriV[0].Z, TriV[1].Z, TriV[2].Z);
			if (TriMaxZ < ZPlaneLow - 1.0e-4)
			{
				continue;
			}

			for (int32 EdgeIdx = 0; EdgeIdx < 3; ++EdgeIdx)
			{
				const FVector3d& E0 = TriV[EdgeIdx];
				const FVector3d& E1 = TriV[(EdgeIdx + 1) % 3];
				if (TriMinZ <= ZPlaneLow && TriMaxZ >= ZPlaneLow)
				{
					AccumulateEdgePlaneCrossing(E0, E1, ZPlaneLow);
				}
				if (TriMinZ <= ZPlaneMid && TriMaxZ >= ZPlaneMid)
				{
					AccumulateEdgePlaneCrossing(E0, E1, ZPlaneMid);
				}
			}
		}

		return TopBandBounds.IsEmpty() ? FullBounds : TopBandBounds;
	}

	bool DoesTriangleIntersectAABB3d(
		const UE::Geometry::FTriangle3d& Tri,
		const UE::Geometry::FAxisAlignedBox3d& Box)
	{
		constexpr double SatEps = 1.0e-9;
		const FVector3d BoxCenter = Box.Center();
		const FVector3d HalfExtents = Box.Extents();

		const FVector3d V0 = Tri.V[0] - BoxCenter;
		const FVector3d V1 = Tri.V[1] - BoxCenter;
		const FVector3d V2 = Tri.V[2] - BoxCenter;

		for (int32 AxisIdx = 0; AxisIdx < 3; ++AxisIdx)
		{
			const double MinProj = FMath::Min3(V0[AxisIdx], V1[AxisIdx], V2[AxisIdx]);
			const double MaxProj = FMath::Max3(V0[AxisIdx], V1[AxisIdx], V2[AxisIdx]);
			if (MinProj > HalfExtents[AxisIdx] + SatEps || MaxProj < -HalfExtents[AxisIdx] - SatEps)
			{
				return false;
			}
		}

		const FVector3d Edges[3] = {
			V1 - V0,
			V2 - V1,
			V0 - V2
		};

		auto IsSeparatedOnAxis = [&V0, &V1, &V2, &HalfExtents](const FVector3d& Axis) -> bool
		{
			if (Axis.SquaredLength() <= 1.0e-16)
			{
				return false;
			}

			const double P0 = Axis.Dot(V0);
			const double P1 = Axis.Dot(V1);
			const double P2 = Axis.Dot(V2);
			const double BoxRadius =
				HalfExtents.X * FMath::Abs(Axis.X) +
				HalfExtents.Y * FMath::Abs(Axis.Y) +
				HalfExtents.Z * FMath::Abs(Axis.Z) +
				SatEps;

			return (FMath::Min3(P0, P1, P2) > BoxRadius) || (FMath::Max3(P0, P1, P2) < -BoxRadius);
		};

		const FVector3d BoxAxes[3] = {
			FVector3d(1.0, 0.0, 0.0),
			FVector3d(0.0, 1.0, 0.0),
			FVector3d(0.0, 0.0, 1.0)
		};

		for (int32 i = 0; i < 3; ++i)
		{
			for (int32 j = 0; j < 3; ++j)
			{
				if (IsSeparatedOnAxis(BoxAxes[i].Cross(Edges[j])))
				{
					return false;
				}
			}
		}

		return !IsSeparatedOnAxis(Edges[0].Cross(Edges[1]));
	}

	void BuildWatertightExtrudedProfileX(
		UE::Geometry::FDynamicMesh3& OutMesh,
		const TArray<FVector2d>& ProfileYZ,
		double HalfExtentX)
	{
		OutMesh.Clear();
		OutMesh.EnableAttributes();
		OutMesh.Attributes()->EnableMaterialID();

		const int32 NumProfileVerts = ProfileYZ.Num();
		if (NumProfileVerts < 3)
		{
			return;
		}

		UE::Geometry::TPolygon2<double> OuterPolygon(ProfileYZ);
		if (OuterPolygon.SignedArea() < 0.0)
		{
			OuterPolygon.Reverse();
		}

		const UE::Geometry::FGeneralPolygon2d GeneralPoly(OuterPolygon);
		const TArray<UE::Geometry::FIndex3i> CapTris = UE::Geometry::ConstrainedDelaunayTriangulate<double>(GeneralPoly);

		const TArray<FVector2d>& OrderedVerts = OuterPolygon.GetVertices();
		TArray<int32> FrontVids;
		TArray<int32> BackVids;
		FrontVids.Reserve(NumProfileVerts);
		BackVids.Reserve(NumProfileVerts);

		for (int32 i = 0; i < NumProfileVerts; ++i)
		{
			const FVector2d& Pt = OrderedVerts[i];
			FrontVids.Add(OutMesh.AppendVertex(FVector3d(-HalfExtentX, Pt.X, Pt.Y)));
			BackVids.Add(OutMesh.AppendVertex(FVector3d(+HalfExtentX, Pt.X, Pt.Y)));
		}

		for (const UE::Geometry::FIndex3i& Tri : CapTris)
		{
			int32 Ia = Tri.A;
			int32 Ib = Tri.B;
			int32 Ic = Tri.C;
			const FVector2d& Va = OrderedVerts[Ia];
			const FVector2d& Vb = OrderedVerts[Ib];
			const FVector2d& Vc = OrderedVerts[Ic];
			const double SignedCross2D = ((Vb.X - Va.X) * (Vc.Y - Va.Y)) - ((Vb.Y - Va.Y) * (Vc.X - Va.X));
			if (SignedCross2D < 0.0)
			{
				Swap(Ib, Ic);
			}

			OutMesh.AppendTriangle(FrontVids[Ia], FrontVids[Ic], FrontVids[Ib]);
			OutMesh.AppendTriangle(BackVids[Ia], BackVids[Ib], BackVids[Ic]);
		}

		for (int32 i = 0; i < NumProfileVerts; ++i)
		{
			const int32 Next = (i + 1) % NumProfileVerts;
			OutMesh.AppendTriangle(FrontVids[i], FrontVids[Next], BackVids[Next]);
			OutMesh.AppendTriangle(FrontVids[i], BackVids[Next], BackVids[i]);
		}

		if (UE::Geometry::TMeshQueries<UE::Geometry::FDynamicMesh3>::GetVolumeArea(OutMesh).X < 0.0)
		{
			OutMesh.ReverseOrientation();
		}

		if (UE::Geometry::FDynamicMeshNormalOverlay* NormalOverlay = OutMesh.Attributes()->PrimaryNormals())
		{
			for (int32 Tid : OutMesh.TriangleIndicesItr())
			{
				const FVector3f TriNormal = static_cast<FVector3f>(OutMesh.GetTriNormal(Tid));
				const int32 E0 = NormalOverlay->AppendElement(TriNormal);
				const int32 E1 = NormalOverlay->AppendElement(TriNormal);
				const int32 E2 = NormalOverlay->AppendElement(TriNormal);
				NormalOverlay->SetTriangle(Tid, UE::Geometry::FIndex3i(E0, E1, E2));
			}
		}
	}

	void BuildProceduralMonolithShape(
		UDynamicMesh* TargetDynMesh,
		EMonolithBaseShape Shape,
		const FVector& DimensionsCm)
	{
		const float DimX = FMath::Max(5.0f, static_cast<float>(DimensionsCm.X));
		const float DimY = FMath::Max(5.0f, static_cast<float>(DimensionsCm.Y));
		const float DimZ = FMath::Max(5.0f, static_cast<float>(DimensionsCm.Z));

		FGeometryScriptPrimitiveOptions PrimOptions;
		PrimOptions.PolygroupMode = EGeometryScriptPrimitivePolygroupMode::PerFace;
		PrimOptions.UVMode = EGeometryScriptPrimitiveUVMode::Uniform;

		switch (Shape)
		{
		case EMonolithBaseShape::Box:
		{
			UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendBox(
				TargetDynMesh,
				PrimOptions,
				FTransform::Identity,
				DimX,
				DimY,
				DimZ,
				0,
				0,
				0,
				EGeometryScriptPrimitiveOriginMode::Center,
				nullptr);
			break;
		}
		case EMonolithBaseShape::OctagonalPrism:
		{
			const float RadiusX = 0.5f * DimX;
			const double ScaleY = static_cast<double>(DimY) / FMath::Max(1.0, static_cast<double>(DimX));
			const FTransform PrismTransform(FQuat::Identity, FVector::ZeroVector, FVector(1.0, ScaleY, 1.0));

			UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendCylinder(
				TargetDynMesh,
				PrimOptions,
				PrismTransform,
				RadiusX,
				DimZ,
				8,
				0,
				true,
				EGeometryScriptPrimitiveOriginMode::Center,
				nullptr);
			break;
		}
		case EMonolithBaseShape::AsymmetricLBlock:
		{
			const double Hx = 0.5 * static_cast<double>(DimX);
			const double YMin = -0.50 * static_cast<double>(DimY);
			const double YStemMax = +0.12 * static_cast<double>(DimY);
			const double YMax = +0.50 * static_cast<double>(DimY);
			const double ZMin = -0.50 * static_cast<double>(DimZ);
			const double ZFootTop = -0.12 * static_cast<double>(DimZ);
			const double ZMax = +0.50 * static_cast<double>(DimZ);

			// Front-facing YZ L-profile extruded along X: full-height stem grips cleanly around Y=0 while lower L-foot juts right (+Y).
			const TArray<FVector2d> LProfileYZ = {
				FVector2d(YMin, ZMin),
				FVector2d(YMax, ZMin),
				FVector2d(YMax, ZFootTop),
				FVector2d(YStemMax, ZFootTop),
				FVector2d(YStemMax, ZMax),
				FVector2d(YMin, ZMax)
			};

			TargetDynMesh->EditMesh([&LProfileYZ, Hx](UE::Geometry::FDynamicMesh3& EditMesh)
			{
				BuildWatertightExtrudedProfileX(EditMesh, LProfileYZ, Hx);
			});
			break;
		}
		case EMonolithBaseShape::CantileveredJHook:
		{
			const double Hx = 0.5 * static_cast<double>(DimX);
			const double Sy = static_cast<double>(DimY);
			const double Sz = static_cast<double>(DimZ);

			// Strictly Z-monotone CCW J-hook profile: eccentric left clamp stem + heavy lower-right cantilevered J-belly (zero floating islands on horizontal cuts).
			const TArray<FVector2d> JHookProfileYZ = {
				FVector2d(-0.32 * Sy, -0.50 * Sz),
				FVector2d(+0.30 * Sy, -0.50 * Sz),
				FVector2d(+0.50 * Sy, -0.36 * Sz),
				FVector2d(+0.50 * Sy, -0.04 * Sz),
				FVector2d(+0.14 * Sy, +0.16 * Sz),
				FVector2d(+0.14 * Sy, +0.50 * Sz),
				FVector2d(-0.50 * Sy, +0.50 * Sz),
				FVector2d(-0.50 * Sy, -0.38 * Sz)
			};

			TargetDynMesh->EditMesh([&JHookProfileYZ, Hx](UE::Geometry::FDynamicMesh3& EditMesh)
			{
				BuildWatertightExtrudedProfileX(EditMesh, JHookProfileYZ, Hx);
			});
			break;
		}
		case EMonolithBaseShape::OffsetCChannel:
		{
			const double Hx = 0.5 * static_cast<double>(DimX);
			const double Sy = static_cast<double>(DimY);
			const double Sz = static_cast<double>(DimZ);

			// Strictly Z-monotone CCW offset C-channel profile: continuous right spine + sloped C-throat recess & lower flange housing Vein 0.
			const TArray<FVector2d> CChannelProfileYZ = {
				FVector2d(-0.50 * Sy, -0.50 * Sz),
				FVector2d(+0.36 * Sy, -0.50 * Sz),
				FVector2d(+0.50 * Sy, -0.40 * Sz),
				FVector2d(+0.50 * Sy, +0.50 * Sz),
				FVector2d(-0.17 * Sy, +0.50 * Sz),
				FVector2d(-0.17 * Sy, +0.30 * Sz),
				FVector2d(-0.48 * Sy, +0.20 * Sz),
				FVector2d(-0.48 * Sy, +0.05 * Sz),
				FVector2d(+0.04 * Sy, -0.04 * Sz),
				FVector2d(+0.04 * Sy, -0.19 * Sz),
				FVector2d(-0.50 * Sy, -0.25 * Sz)
			};

			TargetDynMesh->EditMesh([&CChannelProfileYZ, Hx](UE::Geometry::FDynamicMesh3& EditMesh)
			{
				BuildWatertightExtrudedProfileX(EditMesh, CChannelProfileYZ, Hx);
			});
			break;
		}
		case EMonolithBaseShape::CounterweightedZBracket:
		{
			const double Hx = 0.5 * static_cast<double>(DimX);
			const double Sy = static_cast<double>(DimY);
			const double Sz = static_cast<double>(DimZ);

			// Strictly Z-monotone CCW counterweighted Z-bracket profile: opposing upper-right (+Y) and lower-left (-Y) bismuth counterweights.
			const TArray<FVector2d> ZBracketProfileYZ = {
				FVector2d(-0.50 * Sy, -0.50 * Sz),
				FVector2d(+0.20 * Sy, -0.50 * Sz),
				FVector2d(+0.20 * Sy, -0.08 * Sz),
				FVector2d(+0.50 * Sy, +0.04 * Sz),
				FVector2d(+0.50 * Sy, +0.26 * Sz),
				FVector2d(+0.135 * Sy, +0.34 * Sz),
				FVector2d(+0.135 * Sy, +0.50 * Sz),
				FVector2d(-0.385 * Sy, +0.50 * Sz),
				FVector2d(-0.385 * Sy, +0.34 * Sz),
				FVector2d(-0.24 * Sy, +0.26 * Sz),
				FVector2d(-0.24 * Sy, +0.04 * Sz),
				FVector2d(-0.50 * Sy, -0.08 * Sz)
			};

			TargetDynMesh->EditMesh([&ZBracketProfileYZ, Hx](UE::Geometry::FDynamicMesh3& EditMesh)
			{
				BuildWatertightExtrudedProfileX(EditMesh, ZBracketProfileYZ, Hx);
			});
			break;
		}
		case EMonolithBaseShape::SteppedCore:
		default:
		{
			const double Hx = 0.5 * static_cast<double>(DimX);
			const double Hy = 0.5 * static_cast<double>(DimY);
			const double Hz = 0.5 * static_cast<double>(DimZ);

			// Front-facing YZ stepped profile extruded along X so horizontal multi-tier steps read silhouettes cleanly from +X camera.
			const TArray<FVector2d> SteppedProfileYZ = {
				FVector2d(-0.62 * Hy, -Hz),
				FVector2d(+0.62 * Hy, -Hz),
				FVector2d(+0.62 * Hy, -0.35 * Hz),
				FVector2d(+Hy, -0.35 * Hz),
				FVector2d(+Hy, +0.35 * Hz),
				FVector2d(+0.52 * Hy, +0.35 * Hz),
				FVector2d(+0.52 * Hy, +Hz),
				FVector2d(-0.52 * Hy, +Hz),
				FVector2d(-0.52 * Hy, +0.35 * Hz),
				FVector2d(-Hy, +0.35 * Hz),
				FVector2d(-Hy, -0.35 * Hz),
				FVector2d(-0.62 * Hy, -0.35 * Hz)
			};

			TargetDynMesh->EditMesh([&SteppedProfileYZ, Hx](UE::Geometry::FDynamicMesh3& EditMesh)
			{
				BuildWatertightExtrudedProfileX(EditMesh, SteppedProfileYZ, Hx);
			});
			break;
		}
		}
	}
}

UMonolithSliceComponent::UMonolithSliceComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UMonolithSliceComponent::BeginPlay()
{
	Super::BeginPlay();
	EnsureRemainderMeshComponent();
}

void UMonolithSliceComponent::SetRemainderMeshComponent(UDynamicMeshComponent* InRemainderMeshComponent)
{
	RemainderMeshComponent = InRemainderMeshComponent;
	if (IsValid(RemainderMeshComponent))
	{
		RemainderMeshComponent->PrimaryComponentTick.bCanEverTick = false;
		RemainderMeshComponent->SetSimulatePhysics(false);
		RemainderMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}

void UMonolithSliceComponent::SetFallbackMaterials(
	UMaterialInterface* InOuterMaterial,
	UMaterialInterface* InCutCapMaterial)
{
	FallbackOuterMaterial = InOuterMaterial;
	FallbackCutCapMaterial = InCutCapMaterial;
}

void UMonolithSliceComponent::RefreshActiveMaterials()
{
	const UMonolithContractDataAsset* Contract = ActiveContract.Get();
	ActiveOuterMaterial = (IsValid(Contract) && IsValid(Contract->OuterShellMaterial))
		? Contract->OuterShellMaterial.Get()
		: FallbackOuterMaterial.Get();
	ActiveCutCapMaterial = (IsValid(Contract) && IsValid(Contract->MoltenCutCapMaterial))
		? Contract->MoltenCutCapMaterial.Get()
		: FallbackCutCapMaterial.Get();

	if (IsValid(RemainderMeshComponent))
	{
		RemainderMeshComponent->ConfigureMaterialSet({ActiveOuterMaterial.Get(), ActiveCutCapMaterial.Get()});
	}
}

void UMonolithSliceComponent::SetClampGripBoxLocal(const FBox& InGripBoxLocal, bool bDisableAutoFit)
{
	ClampGripBoxLocal = InGripBoxLocal;
	if (bDisableAutoFit)
	{
		bAutoFitClampGripBoxToTopOfContract = false;
	}
}

bool UMonolithSliceComponent::InitializeMonolithFromContract(
	const UMonolithContractDataAsset* Contract,
	const UShiftProgressionDataAsset* ShiftConfig)
{
	if (!IsValid(Contract) || !EnsureRemainderMeshComponent())
	{
		return false;
	}

	UDynamicMesh* RemainderDynMesh = RemainderMeshComponent->GetDynamicMesh();
	if (!IsValid(RemainderDynMesh))
	{
		return false;
	}

	ActiveContract = Contract;
	ActiveShiftConfig = ShiftConfig;

	// Step 1: Reset DynamicMesh and configure kinematic non-simulating state on Remainder.
	RemainderDynMesh->Reset();
	RemainderMeshComponent->SetSimulatePhysics(false);
	RemainderMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	UGeometryScriptLibrary_CollisionFunctions::ResetDynamicMeshCollision(RemainderMeshComponent, false, nullptr);

	// Step 2: Populate from CustomStaticMesh (with edge weld + watertight verification) or procedural generator.
	bool bBuiltValidClosedMesh = false;
	if (Contract->BaseShape == EMonolithBaseShape::CustomStaticMesh && IsValid(Contract->CustomMonolithMesh))
	{
		UStaticMesh* LoadedMesh = Contract->CustomMonolithMesh.Get();
		if (IsValid(LoadedMesh))
		{
			FGeometryScriptCopyMeshFromAssetOptions AssetOptions;
			AssetOptions.bApplyBuildSettings = true;
			AssetOptions.bRequestTangents = false;
			AssetOptions.bIgnoreRemoveDegenerates = false;

			FGeometryScriptMeshReadLOD ReadLOD;
			ReadLOD.LODType = EGeometryScriptLODType::MaxAvailable;
			ReadLOD.LODIndex = 0;

			EGeometryScriptOutcomePins CopyOutcome = EGeometryScriptOutcomePins::Failure;
			UGeometryScriptLibrary_StaticMeshFunctions::CopyMeshFromStaticMesh(
				LoadedMesh,
				RemainderDynMesh,
				AssetOptions,
				ReadLOD,
				CopyOutcome,
				nullptr);

			if (CopyOutcome == EGeometryScriptOutcomePins::Success)
			{
				RemainderDynMesh->EditMesh([&bBuiltValidClosedMesh](UE::Geometry::FDynamicMesh3& EditMesh)
				{
					if (!EditMesh.IsClosed())
					{
						UE::Geometry::FMergeCoincidentMeshEdges Welder(&EditMesh);
						Welder.MergeVertexTolerance = CoincidentVertexMergeTolerance;
						Welder.Apply();
					}
					bBuiltValidClosedMesh = EditMesh.IsClosed() && EditMesh.TriangleCount() >= MinValidIslandTriangles;
				});
			}
		}

		if (!bBuiltValidClosedMesh)
		{
			UE_LOG(LogTareAndTithe, Warning,
				TEXT("UMonolithSliceComponent: CustomMonolithMesh on '%s' is missing or non-watertight; falling back to procedural Box."),
				*Contract->GetName());
			RemainderDynMesh->Reset();
		}
	}

	if (!bBuiltValidClosedMesh)
	{
		const EMonolithBaseShape FallbackOrTargetShape =
			(Contract->BaseShape == EMonolithBaseShape::CustomStaticMesh)
			? EMonolithBaseShape::Box
			: Contract->BaseShape;

		BuildProceduralMonolithShape(RemainderDynMesh, FallbackOrTargetShape, Contract->DimensionsCm);
	}

	// Ensure outward-facing UE5 triangle orientation prior to UV projection and volume/CoM integration.
	RemainderDynMesh->EditMesh([](UE::Geometry::FDynamicMesh3& EditMesh)
	{
		if (EditMesh.TriangleCount() >= MinValidIslandTriangles &&
			EditMesh.IsClosed() &&
			UE::Geometry::TMeshQueries<UE::Geometry::FDynamicMesh3>::GetVolumeArea(EditMesh).X < 0.0)
		{
			EditMesh.ReverseOrientation(true);
		}
	});

	// Step 3: Bake static object-space Box-Projection UVs into UV0 once at spawn to prevent texture swimming.
	const double UVScaleCm = FMath::Max(10.0, static_cast<double>(Contract->BoxProjectionUVScaleCm));
	const FTransform BoxProjTransform(FQuat::Identity, FVector::ZeroVector, FVector(UVScaleCm));
	UGeometryScriptLibrary_MeshUVFunctions::SetMeshUVsFromBoxProjection(
		RemainderDynMesh,
		0,
		BoxProjTransform,
		FGeometryScriptMeshSelection(),
		2,
		nullptr);

	// Step 4: Enable MaterialID attribute and stamp Slot 0 (OuterShellMaterial) across all initial triangles.
	UE::Geometry::FAxisAlignedBox3d InitialLocalBounds = UE::Geometry::FAxisAlignedBox3d::Empty();
	UE::Geometry::FAxisAlignedBox3d InitialTopGripBounds = UE::Geometry::FAxisAlignedBox3d::Empty();
	double InitialVolumeCm3 = 0.0;
	FVector3d InitialCoMCm = FVector3d::Zero();
	int32 InitialTriCount = 0;
	const double RequestedTopGripDepthCm = static_cast<double>(TopGripCrossSectionSampleDepthCm);

	RemainderDynMesh->EditMesh([&InitialLocalBounds, &InitialTopGripBounds, &InitialVolumeCm3, &InitialCoMCm, &InitialTriCount, RequestedTopGripDepthCm](
		UE::Geometry::FDynamicMesh3& EditMesh)
	{
		EditMesh.EnableAttributes();
		EditMesh.Attributes()->EnableMaterialID();
		if (UE::Geometry::FDynamicMeshMaterialAttribute* MatIDAttrib = EditMesh.Attributes()->GetMaterialID())
		{
			for (int32 Tid : EditMesh.TriangleIndicesItr())
			{
				MatIDAttrib->SetValue(Tid, 0);
			}
		}

		InitialTriCount = EditMesh.TriangleCount();
		InitialLocalBounds = EditMesh.GetBounds();
		InitialTopGripBounds = ComputeTopGripLocalBounds3d(EditMesh, InitialLocalBounds, RequestedTopGripDepthCm);
		const FVector2d VolArea = UE::Geometry::TMeshQueries<UE::Geometry::FDynamicMesh3>::GetVolumeAreaCenter(
			EditMesh,
			InitialCoMCm);
		InitialVolumeCm3 = VolArea.X;
	});

	if (Contract->BaseShape != EMonolithBaseShape::CustomStaticMesh && InitialTriCount > MaxProceduralTriangleBudget)
	{
		UE_LOG(LogTareAndTithe, Warning,
			TEXT("UMonolithSliceComponent: Procedural monolith '%s' triangle count (%d) exceeded %d budget."),
			*Contract->GetName(), InitialTriCount, MaxProceduralTriangleBudget);
	}

	ActiveOuterMaterial = IsValid(Contract->OuterShellMaterial)
		? Contract->OuterShellMaterial.Get()
		: FallbackOuterMaterial.Get();
	ActiveCutCapMaterial = IsValid(Contract->MoltenCutCapMaterial)
		? Contract->MoltenCutCapMaterial.Get()
		: FallbackCutCapMaterial.Get();
	RemainderMeshComponent->ConfigureMaterialSet({ActiveOuterMaterial.Get(), ActiveCutCapMaterial.Get()});

	if (!InitialLocalBounds.IsEmpty())
	{
		CurrentRemainderLocalBounds = FBox(
			static_cast<FVector>(InitialLocalBounds.Min),
			static_cast<FVector>(InitialLocalBounds.Max));
	}
	else
	{
		CurrentRemainderLocalBounds = FBox(ForceInit);
	}

	if (!InitialTopGripBounds.IsEmpty())
	{
		CurrentTopGripLocalBounds = FBox(
			static_cast<FVector>(InitialTopGripBounds.Min),
			static_cast<FVector>(InitialTopGripBounds.Max));
	}
	else
	{
		CurrentTopGripLocalBounds = CurrentRemainderLocalBounds;
	}

	if (bAutoFitClampGripBoxToTopOfContract && !InitialLocalBounds.IsEmpty())
	{
		const FVector3d FullBoxSize = InitialLocalBounds.Max - InitialLocalBounds.Min;
		const UE::Geometry::FAxisAlignedBox3d EffectiveGripBounds = InitialTopGripBounds.IsEmpty()
			? InitialLocalBounds
			: InitialTopGripBounds;
		const FVector3d GripSize = EffectiveGripBounds.Max - EffectiveGripBounds.Min;
		const FVector3d GripCenter = EffectiveGripBounds.Center();
		const double HalfJawX = FMath::Max(3.0, 0.5 * GripSize.X * static_cast<double>(AutoFitJawHorizontalCoverageRatio));
		const double HalfJawY = FMath::Max(3.0, 0.5 * GripSize.Y * static_cast<double>(AutoFitJawHorizontalCoverageRatio));
		const double JawEmbedZ = FMath::Max(5.0, FullBoxSize.Z * static_cast<double>(AutoFitJawVerticalDepthRatio));

		ClampGripBoxLocal = FBox(
			FVector(GripCenter.X - HalfJawX, GripCenter.Y - HalfJawY, InitialLocalBounds.Max.Z - JawEmbedZ),
			FVector(GripCenter.X + HalfJawX, GripCenter.Y + HalfJawY, InitialLocalBounds.Max.Z + 8.0));
	}

	RemainderMeshComponent->UpdateBounds();

	const double ClampedVolumeCm3 = FMath::Max(0.0, InitialVolumeCm3);
	const double DensityKgPerCm3 = static_cast<double>(Contract->DensityGramsPerCm3) * 1.0e-3;
	CurrentRemainderMassKg = static_cast<float>(ClampedVolumeCm3 * DensityKgPerCm3);
	CurrentRemainderLocalCoMCm = static_cast<FVector>(InitialCoMCm);

	OnMonolithInitialized.Broadcast(CurrentRemainderMassKg, CurrentRemainderLocalCoMCm);
	return InitialTriCount >= MinValidIslandTriangles;
}

FSliceResultSummary UMonolithSliceComponent::ExecuteSlice(
	const FVector& PlaneOriginWorld,
	const FVector& PlaneNormalWorld,
	TArray<FVolatileVeinDefinition>& InOutActiveVeins,
	TArray<ASeveredChunkActor*>& OutSpawnedChunks,
	const FLaserSliceAperture& SheetAperture)
{
	OutSpawnedChunks.Reset();

	FSliceResultSummary Summary;
	Summary.Outcome = ECommitSliceOutcome::InvalidMeshState;
	Summary.NewRemainderMassKg = CurrentRemainderMassKg;
	Summary.NewRemainderLocalCoMCm = CurrentRemainderLocalCoMCm;
	Summary.WorldCutCentroid = PlaneOriginWorld;

	if (!ActiveContract.IsValid() || !EnsureRemainderMeshComponent())
	{
		return Summary;
	}

	UDynamicMesh* RemainderDynMesh = RemainderMeshComponent->GetDynamicMesh();
	if (!IsValid(RemainderDynMesh))
	{
		return Summary;
	}

	const FVector UnitWorldNormal = PlaneNormalWorld.GetSafeNormal();
	if (UnitWorldNormal.IsNearlyZero())
	{
		return Summary;
	}

	const FTransform RemainderTransform = RemainderMeshComponent->GetComponentTransform();
	const FVector3d LocalOrigin = static_cast<FVector3d>(RemainderTransform.InverseTransformPosition(PlaneOriginWorld));
	FVector3d LocalNormal = static_cast<FVector3d>(RemainderTransform.InverseTransformVector(UnitWorldNormal));
	if (!LocalNormal.Normalize())
	{
		return Summary;
	}

	// Step 0: Zero-Allocation Plane + Finite Laser Sheet Aperture Miss Check.
	bool bHasValidMesh = false;
	bool bMissedTarget = false;
	RemainderDynMesh->ProcessMesh([&RemainderTransform, &PlaneOriginWorld, &LocalOrigin, &LocalNormal, &SheetAperture, &bHasValidMesh, &bMissedTarget](
		const UE::Geometry::FDynamicMesh3& SourceMesh)
	{
		if (SourceMesh.TriangleCount() < MinValidIslandTriangles)
		{
			bHasValidMesh = false;
			return;
		}

		bHasValidMesh = true;
		bMissedTarget = !DoesPlaneAndApertureIntersectMesh(
			SourceMesh,
			RemainderTransform,
			PlaneOriginWorld,
			LocalOrigin,
			LocalNormal,
			SheetAperture);
	});

	if (!bHasValidMesh)
	{
		return Summary;
	}

	if (bMissedTarget)
	{
		Summary.Outcome = ECommitSliceOutcome::MissedTarget;
		return Summary;
	}

	// Step 1: Pre-Cut Volatile Vein Detonation Gate (shared canonical evaluator with UVeinScannerComponent).
	const FSlicePreviewTelemetry PreCutVeinTelemetry = UVeinScannerComponent::EvaluateVeinsAgainstLocalPlane(
		static_cast<FVector>(LocalOrigin),
		static_cast<FVector>(LocalNormal),
		true,
		InOutActiveVeins);
	if (PreCutVeinTelemetry.bWillDetonate)
	{
		Summary.Outcome = ECommitSliceOutcome::DetonatedVein;
		OnSliceExecuted.Broadcast(Summary);
		return Summary;
	}

	// Step 2: Dual Half-Space FMeshPlaneCut & MaterialID = 1 Cap Assignment.
	UE::Geometry::FDynamicMesh3 HalfA;
	UE::Geometry::FDynamicMesh3 HalfB;
	RemainderDynMesh->ProcessMesh([&HalfA, &HalfB](const UE::Geometry::FDynamicMesh3& SourceMesh)
	{
		HalfA = SourceMesh;
		HalfB = SourceMesh;
	});

	const UMonolithContractDataAsset* Contract = ActiveContract.Get();
	const float CapUVScaleFactor = 1.0f / FMath::Max(1.0f, Contract->CutCapUVWorldDimensionCm);

	double TotalCutCrossSectionAreaCm2 = 0.0;
	FVector3d WeightedCutCentroidLocal = FVector3d::Zero();

	auto CutAndCapHalfSpace = [&LocalOrigin, CapUVScaleFactor, &TotalCutCrossSectionAreaCm2, &WeightedCutCentroidLocal](
		UE::Geometry::FDynamicMesh3& WorkMesh,
		const FVector3d& CutPlaneNormal,
		bool bAccumulateCapTelemetry)
	{
		WorkMesh.EnableAttributes();
		WorkMesh.Attributes()->EnableMaterialID();

		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		UE::Geometry::FMeshPlaneCut Cutter(&WorkMesh, LocalOrigin, CutPlaneNormal);
		Cutter.UVScaleFactor = CapUVScaleFactor;
		Cutter.Cut();
		Cutter.HoleFill(UE::Geometry::ConstrainedDelaunayTriangulate<double>, true, -1, 1);
		PRAGMA_ENABLE_DEPRECATION_WARNINGS

		UE::Geometry::FDynamicMeshMaterialAttribute* MatIDAttrib = WorkMesh.Attributes()->GetMaterialID();
		for (const TArray<int>& LoopTris : Cutter.HoleFillTriangles)
		{
			for (int Tid : LoopTris)
			{
				if (WorkMesh.IsTriangle(Tid))
				{
					if (MatIDAttrib != nullptr)
					{
						MatIDAttrib->SetValue(Tid, 1);
					}

					if (bAccumulateCapTelemetry)
					{
						const double TriAreaCm2 = WorkMesh.GetTriArea(Tid);
						TotalCutCrossSectionAreaCm2 += TriAreaCm2;
						WeightedCutCentroidLocal += WorkMesh.GetTriCentroid(Tid) * TriAreaCm2;
					}
				}
			}
		}
	};

	CutAndCapHalfSpace(HalfA, LocalNormal, true);
	CutAndCapHalfSpace(HalfB, -LocalNormal, false);

	if (TotalCutCrossSectionAreaCm2 > KINDA_SMALL_NUMBER)
	{
		const FVector3d LocalCutCentroid = WeightedCutCentroidLocal / TotalCutCrossSectionAreaCm2;
		Summary.WorldCutCentroid = RemainderTransform.TransformPosition(static_cast<FVector>(LocalCutCentroid));
	}

	// Step 3, 4, 5, 6, 7: Island Decomposition, BVH + FastWinding Build, Exact Grip Test, Mass/CoM, and Vein Midpoint Partitioning.
	const UE::Geometry::FAxisAlignedBox3d GripBox3d(
		static_cast<FVector3d>(ClampGripBoxLocal.Min),
		static_cast<FVector3d>(ClampGripBoxLocal.Max));

	const int32 InitialVeinCount = InOutActiveVeins.Num();
	TArray<FVector3d> VeinMidpointsLocal;
	VeinMidpointsLocal.Reserve(InitialVeinCount);
	for (const FVolatileVeinDefinition& Vein : InOutActiveVeins)
	{
		VeinMidpointsLocal.Add(static_cast<FVector3d>(Vein.GetMidpointCm()));
	}

	TArray<bool> bVeinInsideGrippedIsland;
	bVeinInsideGrippedIsland.Init(false, InitialVeinCount);

	TArray<bool> bVeinClaimedByDetachedIsland;
	bVeinClaimedByDetachedIsland.Init(false, InitialVeinCount);

	TArray<FExtractedSliceIsland> ExtractedIslands;

	auto DecomposeAndClassifyHalfSpace = [&](UE::Geometry::FDynamicMesh3& WorkMesh)
	{
		if (WorkMesh.TriangleCount() < MinValidIslandTriangles)
		{
			return;
		}

		UE::Geometry::FMeshConnectedComponents ConnectedComponents(&WorkMesh);
		ConnectedComponents.FindConnectedTriangles();

		const int32 NumComponents = ConnectedComponents.Num();
		for (int32 CompIdx = 0; CompIdx < NumComponents; ++CompIdx)
		{
			const UE::Geometry::FMeshConnectedComponents::FComponent& Component = ConnectedComponents.GetComponent(CompIdx);
			if (Component.Indices.Num() < MinValidIslandTriangles)
			{
				continue;
			}

			UE::Geometry::FDynamicSubmesh3 SubmeshExtractor(
				&WorkMesh,
				Component.Indices,
				static_cast<int32>(UE::Geometry::EMeshComponents::All),
				true);

			FExtractedSliceIsland Island;
			Island.Mesh = MoveTemp(SubmeshExtractor.GetSubmesh());
			if (Island.Mesh.TriangleCount() < MinValidIslandTriangles)
			{
				continue;
			}

			// Step 4: Spatial Acceleration Build (AABBTree + FastWinding).
			UE::Geometry::FDynamicMeshAABBTree3 IslandBVH(&Island.Mesh, true);
			UE::Geometry::TFastWindingTree<UE::Geometry::FDynamicMesh3> IslandWinding(&IslandBVH, true);
			Island.LocalBounds = IslandBVH.GetBoundingBox();

			// Step 5: Exact Zero-AABB-Error Clamp Grip Classification.
			bool bGripped = false;
			if (Island.LocalBounds.Intersects(GripBox3d))
			{
				UE::Geometry::FDynamicMeshAABBTree3::FTreeTraversal BoxSatTraversal;
				BoxSatTraversal.NextBoxF = [&GripBox3d, &bGripped](const UE::Geometry::FAxisAlignedBox3d& NodeBox, int32 /*Depth*/) -> bool
				{
					return !bGripped && NodeBox.Intersects(GripBox3d);
				};
				BoxSatTraversal.NextTriangleF = [&Island, &GripBox3d, &bGripped](int32 Tid)
				{
					if (!bGripped)
					{
						UE::Geometry::FTriangle3d Tri3d;
						Island.Mesh.GetTriVertices(Tid, Tri3d.V[0], Tri3d.V[1], Tri3d.V[2]);
						if (DoesTriangleIntersectAABB3d(Tri3d, GripBox3d))
						{
							bGripped = true;
						}
					}
				};
				IslandBVH.DoTraversal(BoxSatTraversal);

				if (!bGripped)
				{
					bGripped = IslandWinding.IsInside(GripBox3d.Center(), FastWindingIsoThreshold);
				}
			}
			Island.bIsGripped = bGripped;

			// Step 6 (Island): Analytical Volume & Local CoM.
			const FVector2d IslandVolArea = UE::Geometry::TMeshQueries<UE::Geometry::FDynamicMesh3>::GetVolumeAreaCenter(
				Island.Mesh,
				Island.LocalCoMCm);
			Island.VolumeCm3 = FMath::Max(0.0, IslandVolArea.X);

			// Step 7: Spatial Vein Midpoint Partitioning before any vertex translation.
			for (int32 VeinIdx = 0; VeinIdx < InitialVeinCount; ++VeinIdx)
			{
				if (Island.LocalBounds.Contains(VeinMidpointsLocal[VeinIdx]) &&
					IslandWinding.IsInside(VeinMidpointsLocal[VeinIdx], FastWindingIsoThreshold))
				{
					if (Island.bIsGripped)
					{
						bVeinInsideGrippedIsland[VeinIdx] = true;
					}
					else
					{
						bVeinClaimedByDetachedIsland[VeinIdx] = true;
						Island.bIsContaminated = true;
					}
				}
			}

			ExtractedIslands.Add(MoveTemp(Island));
		}
	};

	DecomposeAndClassifyHalfSpace(HalfA);
	DecomposeAndClassifyHalfSpace(HalfB);

	// Update active veins on Remainder in-place.
	if (InitialVeinCount > 0)
	{
		TArray<FVolatileVeinDefinition> SurvivingRemainderVeins;
		SurvivingRemainderVeins.Reserve(InitialVeinCount);
		for (int32 VeinIdx = 0; VeinIdx < InitialVeinCount; ++VeinIdx)
		{
			if (bVeinInsideGrippedIsland[VeinIdx] && !bVeinClaimedByDetachedIsland[VeinIdx])
			{
				SurvivingRemainderVeins.Add(InOutActiveVeins[VeinIdx]);
			}
		}
		InOutActiveVeins = MoveTemp(SurvivingRemainderVeins);
	}

	// Merge all Gripped islands into RemainderMesh preserving UV0 and MaterialID attributes.
	UE::Geometry::FDynamicMesh3 MergedRemainderMesh;
	MergedRemainderMesh.EnableAttributes();
	MergedRemainderMesh.Attributes()->EnableMaterialID();

	bool bInitializedMergedRemainder = false;
	for (FExtractedSliceIsland& Island : ExtractedIslands)
	{
		if (!Island.bIsGripped)
		{
			continue;
		}

		if (!bInitializedMergedRemainder)
		{
			MergedRemainderMesh = MoveTemp(Island.Mesh);
			MergedRemainderMesh.EnableAttributes();
			MergedRemainderMesh.Attributes()->EnableMaterialID();
			bInitializedMergedRemainder = true;
		}
		else
		{
			UE::Geometry::FDynamicMeshEditor RemainderEditor(&MergedRemainderMesh);
			UE::Geometry::FMeshIndexMappings IndexMappings;
			RemainderEditor.AppendMesh(&Island.Mesh, IndexMappings);
		}
	}

	const double DensityKgPerCm3 = static_cast<double>(Contract->DensityGramsPerCm3) * 1.0e-3;

	if (MergedRemainderMesh.TriangleCount() >= MinValidIslandTriangles)
	{
		FVector3d RemLocalCoMCm = FVector3d::Zero();
		const FVector2d RemVolArea = UE::Geometry::TMeshQueries<UE::Geometry::FDynamicMesh3>::GetVolumeAreaCenter(
			MergedRemainderMesh,
			RemLocalCoMCm);
		const UE::Geometry::FAxisAlignedBox3d RemBounds = MergedRemainderMesh.GetBounds();

		const double RemVolumeCm3 = FMath::Max(0.0, RemVolArea.X);
		CurrentRemainderMassKg = static_cast<float>(RemVolumeCm3 * DensityKgPerCm3);
		CurrentRemainderLocalCoMCm = static_cast<FVector>(RemLocalCoMCm);
		CurrentRemainderLocalBounds = RemBounds.IsEmpty()
			? FBox(ForceInit)
			: FBox(static_cast<FVector>(RemBounds.Min), static_cast<FVector>(RemBounds.Max));
		RemainderDynMesh->SetMesh(MoveTemp(MergedRemainderMesh));
		RemainderMeshComponent->ConfigureMaterialSet({ActiveOuterMaterial.Get(), ActiveCutCapMaterial.Get()});
		RemainderMeshComponent->UpdateBounds();
	}
	else
	{
		CurrentRemainderMassKg = 0.0f;
		CurrentRemainderLocalCoMCm = FVector::ZeroVector;
		CurrentRemainderLocalBounds = FBox(ForceInit);
		InOutActiveVeins.Reset();
		RemainderDynMesh->Reset();
		RemainderMeshComponent->ConfigureMaterialSet({ActiveOuterMaterial.Get(), ActiveCutCapMaterial.Get()});
		RemainderMeshComponent->UpdateBounds();
	}

	if (!InOutActiveVeins.IsEmpty())
	{
		if (!CurrentRemainderLocalBounds.IsValid)
		{
			InOutActiveVeins.Reset();
		}
		else
		{
			const FBox SafeSurvivingBounds = CurrentRemainderLocalBounds.ExpandBy(2.0);
			InOutActiveVeins.RemoveAll([&SafeSurvivingBounds](const FVolatileVeinDefinition& CandidateVein)
			{
				return !SafeSurvivingBounds.IsInsideOrOn(CandidateVein.GetMidpointCm());
			});
		}
	}

	// Step 8: Sliver Vaporization vs. CoM-Recentered Pooled Chaos Chunk Spawn.
	const UShiftProgressionDataAsset* ShiftCfg = ActiveShiftConfig.Get();
	const double MinSliverVolumeCm3 = IsValid(ShiftCfg)
		? static_cast<double>(ShiftCfg->MinSliverVolumeCm3)
		: static_cast<double>(FallbackMinSliverVolumeCm3);
	const float MinAxisThicknessCm = IsValid(ShiftCfg)
		? FMath::Max(0.10f, ShiftCfg->MinChunkAxisThicknessCm)
		: FMath::Max(0.10f, FallbackMinChunkAxisThicknessCm);
	const float SeparationSpeedCmPerSec = IsValid(ShiftCfg)
		? ShiftCfg->ChunkSeparationSpeedCmPerSec
		: FallbackChunkSeparationSpeedCmPerSec;
	const float WatchdogSeconds = IsValid(ShiftCfg)
		? ShiftCfg->MaxInFlightWatchdogSeconds
		: FallbackInFlightWatchdogSeconds;

	UWorld* World = GetWorld();
	UChunkPoolSubsystem* PoolSubsystem = IsValid(World) ? World->GetSubsystem<UChunkPoolSubsystem>() : nullptr;

	for (FExtractedSliceIsland& Island : ExtractedIslands)
	{
		if (Island.bIsGripped)
		{
			continue;
		}

		const float AnalyticalChunkMassKg = static_cast<float>(Island.VolumeCm3 * DensityKgPerCm3);
		const FVector WorldChunkCoM = RemainderTransform.TransformPosition(static_cast<FVector>(Island.LocalCoMCm));

		// Vaporize ONLY genuinely tiny slivers (< 20 cm^3 and < 0.15 kg); preserve wide, thin slabs carrying real quota mass.
		if (Island.VolumeCm3 < MinSliverVolumeCm3 && AnalyticalChunkMassKg < MeaningfulChunkMassPreservationKg)
		{
			Summary.VaporizedSliverMassKg += AnalyticalChunkMassKg;
			OnSliverVaporized.Broadcast(AnalyticalChunkMassKg, WorldChunkCoM);
			continue;
		}

		if (!IsValid(PoolSubsystem))
		{
			Summary.VaporizedSliverMassKg += AnalyticalChunkMassKg;
			OnSliverVaporized.Broadcast(AnalyticalChunkMassKg, WorldChunkCoM);
			continue;
		}

		// Recenter vertices around (0,0,0) so Chaos inertia tensor & pivot align with analytical CoM without altering baked UV0.
		MeshTransforms::Translate(Island.Mesh, -Island.LocalCoMCm);

		// Sign-invariant separation velocity (Section 5.1).
		const double HalfSpaceSide = FVector::DotProduct(WorldChunkCoM - PlaneOriginWorld, UnitWorldNormal);
		const float SeparationSign = (HalfSpaceSide >= 0.0) ? 1.0f : -1.0f;
		const FVector SeparationVelocity = UnitWorldNormal * (SeparationSpeedCmPerSec * SeparationSign);

		ASeveredChunkActor* SpawnedChunk = PoolSubsystem->AcquireChunk();
		if (IsValid(SpawnedChunk))
		{
			const FTransform ChunkWorldTransform(RemainderTransform.GetRotation(), WorldChunkCoM, FVector::OneVector);
			SpawnedChunk->SetMinCollisionThicknessCm(MinAxisThicknessCm);
			SpawnedChunk->ActivateFromSlice(
				MoveTemp(Island.Mesh),
				ChunkWorldTransform,
				AnalyticalChunkMassKg,
				Island.bIsContaminated,
				ActiveOuterMaterial,
				ActiveCutCapMaterial,
				SeparationVelocity,
				WatchdogSeconds);

			OutSpawnedChunks.Add(SpawnedChunk);
			Summary.SpawnedChunkMassKg += AnalyticalChunkMassKg;
			Summary.SpawnedChunkCount += 1;
			OnChunkSpawnedFromSlice.Broadcast(SpawnedChunk);
		}
	}

	Summary.Outcome = ECommitSliceOutcome::Success;
	Summary.NewRemainderMassKg = CurrentRemainderMassKg;
	Summary.NewRemainderLocalCoMCm = CurrentRemainderLocalCoMCm;
	Summary.CutCrossSectionAreaCm2 = static_cast<float>(TotalCutCrossSectionAreaCm2);

	OnSliceExecuted.Broadcast(Summary);
	return Summary;
}

bool UMonolithSliceComponent::DoesPlaneIntersectRemainder(
	const FVector& PlaneOriginWorld,
	const FVector& PlaneNormalWorld,
	const FLaserSliceAperture& SheetAperture) const
{
	if (!IsValid(RemainderMeshComponent) || CurrentRemainderMassKg <= 0.0f)
	{
		return false;
	}

	const UDynamicMesh* RemainderDynMesh = RemainderMeshComponent->GetDynamicMesh();
	if (!IsValid(RemainderDynMesh))
	{
		return false;
	}

	const FVector UnitWorldNormal = PlaneNormalWorld.GetSafeNormal();
	if (UnitWorldNormal.IsNearlyZero())
	{
		return false;
	}

	const FTransform RemainderTransform = RemainderMeshComponent->GetComponentTransform();
	const FVector3d LocalOrigin = static_cast<FVector3d>(RemainderTransform.InverseTransformPosition(PlaneOriginWorld));
	FVector3d LocalNormal = static_cast<FVector3d>(RemainderTransform.InverseTransformVector(UnitWorldNormal));
	if (!LocalNormal.Normalize())
	{
		return false;
	}

	bool bIntersects = false;
	RemainderDynMesh->ProcessMesh([&RemainderTransform, &PlaneOriginWorld, &LocalOrigin, &LocalNormal, &SheetAperture, &bIntersects](
		const UE::Geometry::FDynamicMesh3& SourceMesh)
	{
		if (SourceMesh.TriangleCount() < MinValidIslandTriangles)
		{
			return;
		}

		bIntersects = DoesPlaneAndApertureIntersectMesh(
			SourceMesh,
			RemainderTransform,
			PlaneOriginWorld,
			LocalOrigin,
			LocalNormal,
			SheetAperture);
	});

	return bIntersects;
}

bool UMonolithSliceComponent::ComputeSeveredHalfSpacePlaneLocal(
	const FVector& PlaneOriginWorld,
	const FVector& PlaneNormalWorld,
	FVector& OutLocalOrigin,
	FVector& OutSeveredLocalNormal) const
{
	OutLocalOrigin = FVector::ZeroVector;
	OutSeveredLocalNormal = FVector::ZeroVector;

	if (CurrentRemainderMassKg <= 0.0f || !IsValid(RemainderMeshComponent))
	{
		return false;
	}

	const FVector UnitWorldNormal = PlaneNormalWorld.GetSafeNormal();
	if (UnitWorldNormal.IsNearlyZero())
	{
		return false;
	}

	const FTransform RemainderTransform = RemainderMeshComponent->GetComponentTransform();
	const FVector3d LocalOrigin = static_cast<FVector3d>(RemainderTransform.InverseTransformPosition(PlaneOriginWorld));
	FVector3d LocalNormal = static_cast<FVector3d>(RemainderTransform.InverseTransformVector(UnitWorldNormal));
	if (!LocalNormal.Normalize())
	{
		return false;
	}

	// Orient SeveredNormal strictly away from the overhead clamp grip center (blade-roll invariant).
	const FVector3d GripCenterLocal = ClampGripBoxLocal.IsValid
		? static_cast<FVector3d>(ClampGripBoxLocal.GetCenter())
		: FVector3d(0.0, 0.0, CurrentRemainderLocalBounds.IsValid ? static_cast<double>(CurrentRemainderLocalBounds.Max.Z) : 40.0);
	const double GripSideSignedDist = (GripCenterLocal - LocalOrigin).Dot(LocalNormal);
	FVector3d SeveredNormal = LocalNormal;
	if (GripSideSignedDist > 1.0e-4)
	{
		SeveredNormal = -LocalNormal;
	}
	else if (GripSideSignedDist >= -1.0e-4)
	{
		SeveredNormal = (LocalNormal.Z >= 0.0) ? -LocalNormal : LocalNormal;
	}

	OutLocalOrigin = static_cast<FVector>(LocalOrigin);
	OutSeveredLocalNormal = static_cast<FVector>(SeveredNormal);
	return true;
}

bool UMonolithSliceComponent::EstimateSeveredMassKg(
	const FVector& PlaneOriginWorld,
	const FVector& PlaneNormalWorld,
	float& OutSeveredMassKg,
	float& OutRemainderMassKg) const
{
	OutSeveredMassKg = 0.0f;
	OutRemainderMassKg = CurrentRemainderMassKg;

	const UMonolithContractDataAsset* Contract = ActiveContract.Get();
	if (!IsValid(Contract) || CurrentRemainderMassKg <= 0.0f || !IsValid(RemainderMeshComponent))
	{
		return false;
	}

	const UDynamicMesh* RemainderDynMesh = RemainderMeshComponent->GetDynamicMesh();
	if (!IsValid(RemainderDynMesh))
	{
		return false;
	}

	FVector ComputedLocalOrigin = FVector::ZeroVector;
	FVector ComputedSeveredNormal = FVector::ZeroVector;
	if (!ComputeSeveredHalfSpacePlaneLocal(PlaneOriginWorld, PlaneNormalWorld, ComputedLocalOrigin, ComputedSeveredNormal))
	{
		return false;
	}

	const FVector3d LocalOrigin = static_cast<FVector3d>(ComputedLocalOrigin);
	const FVector3d SeveredNormal = static_cast<FVector3d>(ComputedSeveredNormal);

	// Zero-allocation Planar-Origin Divergence Theorem half-space clipped volume integration.
	// Shifting vertices by -LocalOrigin places the cut plane through (0,0,0), making planar cap volume identically 0.
	bool bHasValidMesh = false;
	double SixTimesSeveredVolumeCm3 = 0.0;

	RemainderDynMesh->ProcessMesh([&LocalOrigin, &SeveredNormal, &bHasValidMesh, &SixTimesSeveredVolumeCm3](
		const UE::Geometry::FDynamicMesh3& SourceMesh)
	{
		if (SourceMesh.TriangleCount() < MinValidIslandTriangles)
		{
			return;
		}

		bHasValidMesh = true;
		double AccumSixVol = 0.0;

		for (int32 Tid : SourceMesh.TriangleIndicesItr())
		{
			FVector3d V[3];
			SourceMesh.GetTriVertices(Tid, V[0], V[1], V[2]);

			const FVector3d P[3] = {
				V[0] - LocalOrigin,
				V[1] - LocalOrigin,
				V[2] - LocalOrigin
			};

			const double D[3] = {
				P[0].Dot(SeveredNormal),
				P[1].Dot(SeveredNormal),
				P[2].Dot(SeveredNormal)
			};

			const bool bPos[3] = { D[0] > 0.0, D[1] > 0.0, D[2] > 0.0 };
			const int32 PosCount = (bPos[0] ? 1 : 0) + (bPos[1] ? 1 : 0) + (bPos[2] ? 1 : 0);

			if (PosCount == 0)
			{
				continue;
			}
			if (PosCount == 3)
			{
				AccumSixVol += SignedTetSixVolumeUE(P[0], P[1], P[2]);
				continue;
			}
			if (PosCount == 1)
			{
				// Guard A: Preserve cyclic vertex winding (i, (i+1)%3, (i+2)%3) for the single positive vertex i.
				const int32 i = bPos[0] ? 0 : (bPos[1] ? 1 : 2);
				const int32 j = (i + 1) % 3;
				const int32 k = (i + 2) % 3;

				const double Tij = D[i] / (D[i] - D[j]);
				const double Tik = D[i] / (D[i] - D[k]);
				const FVector3d Qij = P[i] + Tij * (P[j] - P[i]);
				const FVector3d Qik = P[i] + Tik * (P[k] - P[i]);

				AccumSixVol += SignedTetSixVolumeUE(P[i], Qij, Qik);
			}
			else // PosCount == 2
			{
				// Guard A: Preserve cyclic vertex winding (i, (i+1)%3, (i+2)%3) around the single non-positive vertex i.
				const int32 i = !bPos[0] ? 0 : (!bPos[1] ? 1 : 2);
				const int32 j = (i + 1) % 3;
				const int32 k = (i + 2) % 3;

				const double Tji = D[j] / (D[j] - D[i]);
				const double Tki = D[k] / (D[k] - D[i]);
				const FVector3d Qji = P[j] + Tji * (P[i] - P[j]);
				const FVector3d Qki = P[k] + Tki * (P[i] - P[k]);

				AccumSixVol += SignedTetSixVolumeUE(P[j], P[k], Qki);
				AccumSixVol += SignedTetSixVolumeUE(P[j], Qki, Qji);
			}
		}

		SixTimesSeveredVolumeCm3 = AccumSixVol;
	});

	if (!bHasValidMesh)
	{
		return false;
	}

	const double DensityKgPerCm3 = static_cast<double>(Contract->DensityGramsPerCm3) * 1.0e-3;
	const double ClippedVolumeCm3 = FMath::Max(0.0, SixTimesSeveredVolumeCm3 / 6.0);
	OutSeveredMassKg = FMath::Clamp(static_cast<float>(ClippedVolumeCm3 * DensityKgPerCm3), 0.0f, CurrentRemainderMassKg);
	OutRemainderMassKg = FMath::Max(0.0f, CurrentRemainderMassKg - OutSeveredMassKg);
	return true;
}

FBox UMonolithSliceComponent::GetRemainderWorldBounds() const
{
	if (IsValid(RemainderMeshComponent) && CurrentRemainderMassKg > 0.0f)
	{
		if (CurrentRemainderLocalBounds.IsValid)
		{
			return CurrentRemainderLocalBounds.TransformBy(RemainderMeshComponent->GetComponentTransform());
		}
		return RemainderMeshComponent->Bounds.GetBox();
	}
	return FBox(ForceInit);
}

bool UMonolithSliceComponent::EnsureRemainderMeshComponent()
{
	if (IsValid(RemainderMeshComponent))
	{
		return true;
	}

	if (AActor* OwnerActor = GetOwner())
	{
		if (UDynamicMeshComponent* FoundComp = OwnerActor->FindComponentByClass<UDynamicMeshComponent>())
		{
			SetRemainderMeshComponent(FoundComp);
			return true;
		}
	}

	return false;
}
