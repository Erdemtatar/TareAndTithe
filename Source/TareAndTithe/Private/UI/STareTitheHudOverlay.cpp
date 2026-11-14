// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/STareTitheHudOverlay.h"
#include "HAL/IConsoleManager.h"
#include "Layout/ArrangedChildren.h"
#include "Layout/Geometry.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	TAutoConsoleVariable<FString> CVarDiagControlsCardJson(
		TEXT("TareTithe.Diag.ControlsCardJson"),
		TEXT("{}"),
		TEXT("Read-only Phase 12-FIX5 Controls Card typography/width JSON"),
		ECVF_Default);
}

namespace TareTitheHudPaint
{
	static const FSlateBrush* GetWhiteBrush()
	{
		return FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));
	}

	static FLinearColor WithAlpha(const FLinearColor& Base, float Alpha)
	{
		return FLinearColor(Base.R, Base.G, Base.B, FMath::Clamp(Alpha, 0.0f, 1.0f));
	}

	static void DrawSolidBox(
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FGeometry& Geom,
		const FVector2D& Pos,
		const FVector2D& Size,
		const FLinearColor& Color)
	{
		if (Size.X <= 0.0f || Size.Y <= 0.0f || Color.A <= KINDA_SMALL_NUMBER)
		{
			return;
		}
		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId,
			Geom.ToPaintGeometry(Size, FSlateLayoutTransform(Pos)),
			GetWhiteBrush(),
			ESlateDrawEffect::None,
			Color);
	}

	static void DrawPerimeterBorder(
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FGeometry& Geom,
		const FVector2D& Pos,
		const FVector2D& Size,
		float Thickness,
		const FLinearColor& BorderColor)
	{
		if (Size.X <= 2.0f * Thickness || Size.Y <= 2.0f * Thickness || BorderColor.A <= KINDA_SMALL_NUMBER)
		{
			return;
		}
		DrawSolidBox(OutDrawElements, LayerId, Geom, Pos, FVector2D(Size.X, Thickness), BorderColor);
		DrawSolidBox(OutDrawElements, LayerId, Geom, FVector2D(Pos.X, Pos.Y + Size.Y - Thickness), FVector2D(Size.X, Thickness), BorderColor);
		DrawSolidBox(OutDrawElements, LayerId, Geom, FVector2D(Pos.X, Pos.Y + Thickness), FVector2D(Thickness, Size.Y - 2.0f * Thickness), BorderColor);
		DrawSolidBox(OutDrawElements, LayerId, Geom, FVector2D(Pos.X + Size.X - Thickness, Pos.Y + Thickness), FVector2D(Thickness, Size.Y - 2.0f * Thickness), BorderColor);
	}

	static void DrawCornerBrackets(
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FGeometry& Geom,
		const FVector2D& Size,
		float ArmLen,
		float Thickness,
		const FLinearColor& Color)
	{
		if (Size.X < 2.5f * ArmLen || Size.Y < 2.5f * ArmLen || Color.A <= KINDA_SMALL_NUMBER)
		{
			return;
		}
		DrawSolidBox(OutDrawElements, LayerId, Geom, FVector2D(0.0f, 0.0f), FVector2D(ArmLen, Thickness), Color);
		DrawSolidBox(OutDrawElements, LayerId, Geom, FVector2D(0.0f, 0.0f), FVector2D(Thickness, ArmLen), Color);
		DrawSolidBox(OutDrawElements, LayerId, Geom, FVector2D(Size.X - ArmLen, 0.0f), FVector2D(ArmLen, Thickness), Color);
		DrawSolidBox(OutDrawElements, LayerId, Geom, FVector2D(Size.X - Thickness, 0.0f), FVector2D(Thickness, ArmLen), Color);
		DrawSolidBox(OutDrawElements, LayerId, Geom, FVector2D(0.0f, Size.Y - Thickness), FVector2D(ArmLen, Thickness), Color);
		DrawSolidBox(OutDrawElements, LayerId, Geom, FVector2D(0.0f, Size.Y - ArmLen), FVector2D(Thickness, ArmLen), Color);
		DrawSolidBox(OutDrawElements, LayerId, Geom, FVector2D(Size.X - ArmLen, Size.Y - Thickness), FVector2D(ArmLen, Thickness), Color);
		DrawSolidBox(OutDrawElements, LayerId, Geom, FVector2D(Size.X - Thickness, Size.Y - ArmLen), FVector2D(Thickness, ArmLen), Color);
	}
}

// ============================================================================
// 1. TOP-LEFT INDENTURE & REQUISITION PANEL (+50% EXPANDED, PROPORTIONAL TYPOGRAPHY)
// ============================================================================
class STopLeftIndenturePanel : public SCompoundWidget
{
public:
	static constexpr float PadLeftPx = 15.0f;
	static constexpr float PadTopPx = 10.0f;
	static constexpr float PadRightPx = 13.0f;
	static constexpr float PadBottomPx = 9.0f;
	static constexpr float HorizontalPadTotalPx = PadLeftPx + PadRightPx;

	SLATE_BEGIN_ARGS(STopLeftIndenturePanel)
	{
		_Visibility = EVisibility::HitTestInvisible;
	}
	SLATE_END_ARGS()

	void Construct(const FArguments& /*InArgs*/)
	{
		SetVisibility(EVisibility::HitTestInvisible);

		const FSlateFontInfo HeaderFont = FCoreStyle::GetDefaultFontStyle("Bold", 13);
		const FSlateFontInfo DirectiveFont = FCoreStyle::GetDefaultFontStyle("Bold", 14);
		const FSlateFontInfo ScripFont = FCoreStyle::GetDefaultFontStyle("Bold", 13);
		const FSlateFontInfo PillFont = FCoreStyle::GetDefaultFontStyle("Bold", 11);

		ChildSlot
		.Padding(FMargin(PadLeftPx, PadTopPx, PadRightPx, PadBottomPx))
		[
			SNew(SVerticalBox)
			.Visibility(EVisibility::HitTestInvisible)

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 5.0f)
			[
				SAssignNew(HeaderTextBlock, STextBlock)
				.Visibility(EVisibility::HitTestInvisible)
				.Font(HeaderFont)
				.ColorAndOpacity(FLinearColor(0.05f, 0.88f, 1.00f, 1.0f))
				.ShadowOffset(FVector2D(1.0f, 1.0f))
				.ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.88f))
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 3.0f, 0.0f, 5.0f)
			[
				SAssignNew(DirectiveTextBlock, STextBlock)
				.Visibility(EVisibility::HitTestInvisible)
				.Font(DirectiveFont)
				.AutoWrapText(true)
				.WrapTextAt(494.0f)
				.ColorAndOpacity(FLinearColor(1.00f, 0.82f, 0.18f, 1.0f))
				.ShadowOffset(FVector2D(1.0f, 1.0f))
				.ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.88f))
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 1.0f, 0.0f, 1.0f)
			[
				SNew(SHorizontalBox)
				.Visibility(EVisibility::HitTestInvisible)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0.0f, 0.0f, 10.0f, 0.0f)
				[
					SAssignNew(ScripTextBlock, STextBlock)
					.Visibility(EVisibility::HitTestInvisible)
					.Font(ScripFont)
					.ColorAndOpacity(FLinearColor(1.00f, 0.82f, 0.18f, 1.0f))
					.ShadowOffset(FVector2D(1.0f, 1.0f))
					.ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.85f))
				]

				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.VAlign(VAlign_Center)
				[
					SNew(SHorizontalBox)
					.Visibility(EVisibility::HitTestInvisible)

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(0.0f, 0.0f, 7.0f, 0.0f)
					[
						SAssignNew(UpgradePillBlocks[0], STextBlock)
						.Visibility(EVisibility::HitTestInvisible)
						.Font(PillFont)
						.ShadowOffset(FVector2D(1.0f, 1.0f))
						.ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.82f))
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(0.0f, 0.0f, 7.0f, 0.0f)
					[
						SAssignNew(UpgradePillBlocks[1], STextBlock)
						.Visibility(EVisibility::HitTestInvisible)
						.Font(PillFont)
						.ShadowOffset(FVector2D(1.0f, 1.0f))
						.ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.82f))
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SAssignNew(UpgradePillBlocks[2], STextBlock)
						.Visibility(EVisibility::HitTestInvisible)
						.Font(PillFont)
						.ShadowOffset(FVector2D(1.0f, 1.0f))
						.ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.82f))
					]
				]
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 3.0f, 0.0f, 0.0f)
			[
				SAssignNew(FeedbackTextBlock, STextBlock)
				.Visibility(EVisibility::Collapsed)
				.Font(PillFont)
				.AutoWrapText(true)
				.WrapTextAt(494.0f)
			]
		];
	}

	void SetConfig(const FHudPresentationConfig& InCfg)
	{
		Config = InCfg;
		CachedHeaderPt = -1;
	}

	void UpdateResponsiveTypography(float EffectiveCornerWidth)
	{
		// Refinement C: Full 13/14/13/11 pt at >= 500 px; proportional FMath::Clamp(EffectiveCornerWidth / 525.0f, 0.80f, 1.0f) scaling below 500 px.
		constexpr float ReferenceWidth = 525.0f;
		const bool bCompactScale = (EffectiveCornerWidth < 500.0f);
		const float FontScale = bCompactScale
			? FMath::Clamp(EffectiveCornerWidth / ReferenceWidth, 0.80f, 1.0f)
			: 1.0f;

		auto ScalePt = [bCompactScale, FontScale](int32 BasePt, int32 MinPt, int32 MaxPt) -> int32
		{
			const int32 RawPt = bCompactScale
				? FMath::FloorToInt(static_cast<float>(BasePt) * FontScale)
				: BasePt;
			return FMath::Clamp(RawPt, MinPt, MaxPt);
		};

		const int32 HeaderPt = ScalePt(Config.CornerHeaderFontSizePt, 9, 22);
		const int32 BodyPt = ScalePt(Config.CornerBodyFontSizePt, 10, 24);
		const int32 SubPt = ScalePt(Config.CornerSubFontSizePt, 9, 22);
		const int32 PillPt = ScalePt(Config.CornerPillFontSizePt, 8, 20);

		if (HeaderPt != CachedHeaderPt || BodyPt != CachedBodyPt || SubPt != CachedSubPt || PillPt != CachedPillPt)
		{
			CachedHeaderPt = HeaderPt;
			CachedBodyPt = BodyPt;
			CachedSubPt = SubPt;
			CachedPillPt = PillPt;

			if (HeaderTextBlock.IsValid())
			{
				HeaderTextBlock->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", HeaderPt));
			}
			if (DirectiveTextBlock.IsValid())
			{
				DirectiveTextBlock->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", BodyPt));
			}
			if (ScripTextBlock.IsValid())
			{
				ScripTextBlock->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", SubPt));
			}
			const FSlateFontInfo ScaledPillFont = FCoreStyle::GetDefaultFontStyle("Bold", PillPt);
			for (int32 Idx = 0; Idx < 3; ++Idx)
			{
				if (UpgradePillBlocks[Idx].IsValid())
				{
					UpgradePillBlocks[Idx]->SetFont(ScaledPillFont);
				}
			}
			if (FeedbackTextBlock.IsValid())
			{
				FeedbackTextBlock->SetFont(ScaledPillFont);
			}
		}

		SetWrapWidth(EffectiveCornerWidth - HorizontalPadTotalPx);
	}

	void SetWrapWidth(float InWrapWidth)
	{
		const float SafeWrap = FMath::Max(180.0f, InWrapWidth);
		if (!FMath::IsNearlyEqual(CachedWrapWidth, SafeWrap, 0.5f))
		{
			CachedWrapWidth = SafeWrap;
			if (DirectiveTextBlock.IsValid())
			{
				DirectiveTextBlock->SetWrapTextAt(SafeWrap);
			}
			if (FeedbackTextBlock.IsValid())
			{
				FeedbackTextBlock->SetWrapTextAt(SafeWrap);
			}
		}
	}

	float GetHeaderBottomLocalY() const
	{
		const float HeaderH = (HeaderTextBlock.IsValid() && HeaderTextBlock->GetDesiredSize().Y > 4.0f)
			? HeaderTextBlock->GetDesiredSize().Y
			: 19.0f;
		return PadTopPx + HeaderH;
	}

	float GetDividerLocalY() const
	{
		return GetHeaderBottomLocalY() + 3.0f;
	}

	void ApplyViewModel(const FTopLeftIndentureViewModel& VM)
	{
		AccentColor = VM.AccentBarColor;

		if (HeaderTextBlock.IsValid())
		{
			HeaderTextBlock->SetText(VM.HeaderText);
		}
		if (DirectiveTextBlock.IsValid())
		{
			DirectiveTextBlock->SetText(VM.DirectiveBodyText);
			DirectiveTextBlock->SetColorAndOpacity(VM.DirectiveTextColor);
		}
		if (ScripTextBlock.IsValid())
		{
			ScripTextBlock->SetText(VM.ScripBalanceText);
		}
		for (int32 Idx = 0; Idx < 3; ++Idx)
		{
			if (UpgradePillBlocks[Idx].IsValid())
			{
				UpgradePillBlocks[Idx]->SetText(VM.UpgradePillTexts[Idx]);
				UpgradePillBlocks[Idx]->SetColorAndOpacity(VM.UpgradePillColors[Idx]);
			}
		}
		if (FeedbackTextBlock.IsValid())
		{
			if (VM.bHasTransientFeedback && !VM.TransientFeedbackText.IsEmpty())
			{
				FeedbackTextBlock->SetText(VM.TransientFeedbackText);
				FeedbackTextBlock->SetColorAndOpacity(VM.TransientFeedbackColor);
				FeedbackTextBlock->SetVisibility(EVisibility::HitTestInvisible);
			}
			else
			{
				FeedbackTextBlock->SetText(FText::GetEmpty());
				FeedbackTextBlock->SetVisibility(EVisibility::Collapsed);
			}
		}
	}

	virtual int32 OnPaint(
		const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override
	{
		const FVector2D Size = AllottedGeometry.GetLocalSize();
		const FLinearColor FillColor = TareTitheHudPaint::WithAlpha(Config.PanelAnthraciteTint, Config.CornerPanelOpacity);
		const FLinearColor BorderColor = Config.PanelBorderGunmetalTint;

		TareTitheHudPaint::DrawSolidBox(OutDrawElements, LayerId, AllottedGeometry, FVector2D::ZeroVector, Size, FillColor);
		TareTitheHudPaint::DrawPerimeterBorder(OutDrawElements, LayerId + 1, AllottedGeometry, FVector2D::ZeroVector, Size, 1.0f, BorderColor);
		TareTitheHudPaint::DrawSolidBox(
			OutDrawElements,
			LayerId + 2,
			AllottedGeometry,
			FVector2D(0.0f, 0.0f),
			FVector2D(4.0f, Size.Y),
			TareTitheHudPaint::WithAlpha(AccentColor, 0.95f));

		const float DividerY = GetDividerLocalY();
		TareTitheHudPaint::DrawSolidBox(
			OutDrawElements,
			LayerId + 1,
			AllottedGeometry,
			FVector2D(PadLeftPx - 2.0f, DividerY),
			FVector2D(FMath::Max(0.0f, Size.X - (HorizontalPadTotalPx - 4.0f)), 1.0f),
			FLinearColor(0.20f, 0.26f, 0.32f, 0.45f));

		return SCompoundWidget::OnPaint(
			Args,
			AllottedGeometry,
			MyCullingRect,
			OutDrawElements,
			LayerId + 3,
			InWidgetStyle,
			bParentEnabled);
	}

private:
	FHudPresentationConfig Config;
	FLinearColor AccentColor = FLinearColor(0.05f, 0.86f, 1.00f, 1.0f);
	float CachedWrapWidth = 494.0f;
	int32 CachedHeaderPt = -1;
	int32 CachedBodyPt = -1;
	int32 CachedSubPt = -1;
	int32 CachedPillPt = -1;

	TSharedPtr<STextBlock> HeaderTextBlock;
	TSharedPtr<STextBlock> DirectiveTextBlock;
	TSharedPtr<STextBlock> ScripTextBlock;
	TSharedPtr<STextBlock> UpgradePillBlocks[3];
	TSharedPtr<STextBlock> FeedbackTextBlock;
};

// ============================================================================
// 2. TOP-RIGHT DIRECTOR OVERLOOK PANEL (+50% EXPANDED, PROPORTIONAL TYPOGRAPHY)
// ============================================================================
// ============================================================================
// 1B. FAR-LEFT READABILITY CONTROLS CARD (326 PX @ 1080P, U_RIGHT <= 0.180, [H] MICRO-PILL)
// ============================================================================
class SLeftControlsCard : public SCompoundWidget
{
public:
	static constexpr float PadLeftPx = 13.0f;
	static constexpr float PadTopPx = 10.0f;
	static constexpr float PadRightPx = 13.0f;
	static constexpr float PadBottomPx = 10.0f;

	static FSlateFontInfo MakeControlsFont(const ANSICHAR* StyleName, float PointSize)
	{
		FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle(StyleName, FMath::RoundToInt32(PointSize));
		Font.Size = PointSize;
		return Font;
	}

	SLATE_BEGIN_ARGS(SLeftControlsCard)
	{
		_Visibility = EVisibility::HitTestInvisible;
	}
	SLATE_END_ARGS()

	void Construct(const FArguments& /*InArgs*/)
	{
		SetVisibility(EVisibility::HitTestInvisible);

		const FSlateFontInfo HeaderFont = MakeControlsFont("Bold", 11.0f);
		const FSlateFontInfo ToggleHintFont = MakeControlsFont("Bold", 9.5f);
		const FSlateFontInfo KeyFont = MakeControlsFont("Bold", 11.0f);
		const FSlateFontInfo ActionFont = MakeControlsFont("Regular", 10.5f);

		struct FControlRowSpec
		{
			const TCHAR* KeyText;
			const TCHAR* ActionText;
		};
		static const FControlRowSpec Rows[8] = {
			{ TEXT("[LMB]"),    TEXT("HOLD SLICE") },
			{ TEXT("[SCROLL]"), TEXT("ROLL (+SHIFT)") },
			{ TEXT("[RMB]"),    TEXT("BEVEL / ABORT") },
			{ TEXT("[SPACE]"),  TEXT("TITHE / TARE") },
			{ TEXT("[E]"),      TEXT("STAMP / NEXT") },
			{ TEXT("[1-3]"),    TEXT("UPGRADES") },
			{ TEXT("HOLD [R]"), TEXT("PURGE CELL") },
			{ TEXT("[F6-F9]"),  TEXT("GFX: HIGH (TSR 85%)") }
		};

		TSharedRef<SVerticalBox> RowsList = SNew(SVerticalBox)
			.Visibility(EVisibility::HitTestInvisible);

		for (int32 Idx = 0; Idx < 8; ++Idx)
		{
			TSharedPtr<STextBlock> KeyBlock;
			TSharedPtr<STextBlock> ActionBlock;
			TSharedPtr<SHorizontalBox> RowBox;
			SVerticalBox::FSlot* RowSlotPtr = nullptr;

			RowsList->AddSlot()
			.Expose(RowSlotPtr)
			.AutoHeight()
			.Padding(0.0f, (Idx == 0) ? 5.5f : 3.5f, 0.0f, 0.0f)
			[
				SAssignNew(RowBox, SHorizontalBox)
				.Visibility(EVisibility::HitTestInvisible)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SAssignNew(KeyBlock, STextBlock)
					.Visibility(EVisibility::HitTestInvisible)
					.Font(KeyFont)
					.Text(FText::FromString(Rows[Idx].KeyText))
					.ColorAndOpacity(FLinearColor(1.00f, 0.80f, 0.18f, 0.96f))
					.ShadowOffset(FVector2D(1.0f, 1.0f))
					.ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.88f))
				]

				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.HAlign(HAlign_Right)
				.VAlign(VAlign_Center)
				.Padding(6.0f, 0.0f, 0.0f, 0.0f)
				[
					SAssignNew(ActionBlock, STextBlock)
					.Visibility(EVisibility::HitTestInvisible)
					.Justification(ETextJustify::Right)
					.Font(ActionFont)
					.Text(FText::FromString(Rows[Idx].ActionText))
					.ColorAndOpacity(
						(Idx == 7)
						? FLinearColor(0.56f, 0.92f, 1.00f, 0.96f)
						: FLinearColor(0.84f, 0.90f, 0.96f, 0.94f))
					.ShadowOffset(FVector2D(1.0f, 1.0f))
					.ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.88f))
				]
			];

			if (Idx == 7)
			{
				GraphicsRowBox = RowBox;
			}
			RowSlots.Add(RowSlotPtr);
			KeyBlocks.Add(KeyBlock);
			ActionBlocks.Add(ActionBlock);
		}

		ChildSlot
		[
			SNew(SVerticalBox)
			.Visibility(EVisibility::HitTestInvisible)

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SAssignNew(ExpandedContainer, SBox)
				.Visibility(EVisibility::HitTestInvisible)
				.Padding(FMargin(PadLeftPx, PadTopPx, PadRightPx, PadBottomPx))
				[
					SNew(SVerticalBox)
					.Visibility(EVisibility::HitTestInvisible)

					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 0.0f, 0.0f, 4.0f)
					[
						SNew(SHorizontalBox)
						.Visibility(EVisibility::HitTestInvisible)

						+ SHorizontalBox::Slot()
						.FillWidth(1.0f)
						.VAlign(VAlign_Center)
						[
							SAssignNew(HeaderTitleBlock, STextBlock)
							.Visibility(EVisibility::HitTestInvisible)
							.Font(HeaderFont)
							.Text(FText::FromString(TEXT("CONTROLS")))
							.ColorAndOpacity(FLinearColor(0.05f, 0.88f, 1.00f, 1.0f))
							.ShadowOffset(FVector2D(1.0f, 1.0f))
							.ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.88f))
						]

						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.Padding(6.0f, 0.0f, 0.0f, 0.0f)
						[
							SAssignNew(HideHintBlock, STextBlock)
							.Visibility(EVisibility::HitTestInvisible)
							.Font(ToggleHintFont)
							.Text(FText::FromString(TEXT("[H] HIDE")))
							.ColorAndOpacity(FLinearColor(0.54f, 0.86f, 1.00f, 0.95f))
							.ShadowOffset(FVector2D(1.0f, 1.0f))
							.ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.85f))
						]
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						RowsList
					]
				]
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SAssignNew(CollapsedPillContainer, SBox)
				.Visibility(EVisibility::Collapsed)
				.Padding(FMargin(11.0f, 5.0f, 11.0f, 5.0f))
				[
					SAssignNew(CollapsedPillTextBlock, STextBlock)
					.Visibility(EVisibility::HitTestInvisible)
					.Font(ToggleHintFont)
					.Text(FText::FromString(TEXT("[H] CONTROLS")))
					.ColorAndOpacity(FLinearColor(0.05f, 0.88f, 1.00f, 0.96f))
					.ShadowOffset(FVector2D(1.0f, 1.0f))
					.ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.88f))
				]
			]
		];
	}

	void SetConfig(const FHudPresentationConfig& InCfg)
	{
		Config = InCfg;
	}

	void SetExpanded(bool bInExpanded)
	{
		bExpanded = bInExpanded;
		if (ExpandedContainer.IsValid())
		{
			ExpandedContainer->SetVisibility(bExpanded ? EVisibility::HitTestInvisible : EVisibility::Collapsed);
		}
		if (CollapsedPillContainer.IsValid())
		{
			CollapsedPillContainer->SetVisibility(bExpanded ? EVisibility::Collapsed : EVisibility::HitTestInvisible);
		}
		Invalidate(EInvalidateWidgetReason::LayoutAndVolatility);
	}

	bool IsExpanded() const { return bExpanded; }

	void SetGraphicsQualityReadout(const FText& InQualityText, bool bInShowRow)
	{
		bShowGraphicsRow = bInShowRow;
		if (ActionBlocks.IsValidIndex(7) && ActionBlocks[7].IsValid())
		{
			ActionBlocks[7]->SetText(InQualityText);
		}
		if (GraphicsRowBox.IsValid())
		{
			GraphicsRowBox->SetVisibility(bShowGraphicsRow ? EVisibility::HitTestInvisible : EVisibility::Collapsed);
		}
		if (RowSlots.IsValidIndex(7) && RowSlots[7] != nullptr)
		{
			const bool bCompact = (CachedRowPt > 0.0f && CachedRowPt < 10.0f);
			const float TopPad = bShowGraphicsRow ? (bCompact ? 2.5f : 3.5f) : 0.0f;
			RowSlots[7]->SetPadding(FMargin(0.0f, TopPad, 0.0f, 0.0f));
		}
		Invalidate(EInvalidateWidgetReason::LayoutAndVolatility);
	}

	void UpdateResponsiveTypography(float EffectiveCardWidth)
	{
		const bool bCompact = (EffectiveCardWidth < 250.0f);
		const float HeaderPt = bCompact ? 9.5f : 11.0f;
		const float HintPt = bCompact ? 8.5f : 9.5f;
		const float KeyPt = bCompact ? 9.0f : 11.0f;
		const float ActionPt = bCompact ? 9.0f : 10.5f;

		const FString DiagJson = FString::Printf(
			TEXT("{\"width\":%.2f,\"compact\":%s,\"header_pt\":%.1f,\"hint_pt\":%.1f,\"key_pt\":%.1f,\"action_pt\":%.1f,\"pad_x\":%.1f,\"pad_y\":%.1f}"),
			EffectiveCardWidth,
			bCompact ? TEXT("true") : TEXT("false"),
			HeaderPt,
			HintPt,
			KeyPt,
			ActionPt,
			PadLeftPx,
			PadTopPx);
		CVarDiagControlsCardJson->Set(*DiagJson, ECVF_SetByCode);

		if (FMath::IsNearlyEqual(HeaderPt, CachedHeaderPt) && FMath::IsNearlyEqual(KeyPt, CachedRowPt))
		{
			return;
		}
		CachedHeaderPt = HeaderPt;
		CachedRowPt = KeyPt;

		if (HeaderTitleBlock.IsValid())
		{
			HeaderTitleBlock->SetFont(MakeControlsFont("Bold", HeaderPt));
		}
		if (HideHintBlock.IsValid())
		{
			HideHintBlock->SetFont(MakeControlsFont("Bold", HintPt));
		}
		if (CollapsedPillTextBlock.IsValid())
		{
			CollapsedPillTextBlock->SetFont(MakeControlsFont("Bold", HintPt));
		}

		for (int32 Idx = 0; Idx < RowSlots.Num(); ++Idx)
		{
			if (RowSlots[Idx] != nullptr)
			{
				const bool bRowHidden = (Idx == 7 && !bShowGraphicsRow);
				const float TopPad = bRowHidden
					? 0.0f
					: (bCompact
						? ((Idx == 0) ? 4.0f : 2.5f)
						: ((Idx == 0) ? 5.5f : 3.5f));
				RowSlots[Idx]->SetPadding(FMargin(0.0f, TopPad, 0.0f, 0.0f));
			}
		}

		const FSlateFontInfo KeyFont = MakeControlsFont("Bold", KeyPt);
		const FSlateFontInfo ActionFont = MakeControlsFont("Regular", ActionPt);
		for (const TSharedPtr<STextBlock>& KeyBlk : KeyBlocks)
		{
			if (KeyBlk.IsValid())
			{
				KeyBlk->SetFont(KeyFont);
			}
		}
		for (const TSharedPtr<STextBlock>& ActBlk : ActionBlocks)
		{
			if (ActBlk.IsValid())
			{
				ActBlk->SetFont(ActionFont);
			}
		}
	}

	virtual int32 OnPaint(
		const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override
	{
		const FVector2D Size = AllottedGeometry.GetLocalSize();
		const float Alpha = bExpanded ? Config.CornerPanelOpacity : FMath::Min(0.82f, Config.CornerPanelOpacity);
		const FLinearColor FillColor = TareTitheHudPaint::WithAlpha(Config.PanelAnthraciteTint, Alpha);
		const FLinearColor BorderColor = Config.PanelBorderGunmetalTint;
		const FLinearColor AccentCyan(0.05f, 0.86f, 1.00f, 0.92f);

		TareTitheHudPaint::DrawSolidBox(OutDrawElements, LayerId, AllottedGeometry, FVector2D::ZeroVector, Size, FillColor);
		TareTitheHudPaint::DrawPerimeterBorder(OutDrawElements, LayerId + 1, AllottedGeometry, FVector2D::ZeroVector, Size, 1.0f, BorderColor);
		TareTitheHudPaint::DrawSolidBox(OutDrawElements, LayerId + 2, AllottedGeometry, FVector2D::ZeroVector, FVector2D(3.0f, Size.Y), AccentCyan);

		if (bExpanded)
		{
			const float HeaderH = (HeaderTitleBlock.IsValid() && HeaderTitleBlock->GetDesiredSize().Y > 4.0f)
				? HeaderTitleBlock->GetDesiredSize().Y
				: 16.0f;
			const float DividerY = PadTopPx + HeaderH + 2.5f;
			TareTitheHudPaint::DrawSolidBox(
				OutDrawElements,
				LayerId + 1,
				AllottedGeometry,
				FVector2D(PadLeftPx - 2.0f, DividerY),
				FVector2D(FMath::Max(0.0f, Size.X - (PadLeftPx + PadRightPx - 4.0f)), 1.0f),
				FLinearColor(0.20f, 0.26f, 0.32f, 0.45f));
		}

		return SCompoundWidget::OnPaint(
			Args,
			AllottedGeometry,
			MyCullingRect,
			OutDrawElements,
			LayerId + 3,
			InWidgetStyle,
			bParentEnabled);
	}

private:
	FHudPresentationConfig Config;
	bool bExpanded = true;
	bool bShowGraphicsRow = true;
	float CachedHeaderPt = -1.0f;
	float CachedRowPt = -1.0f;

	TSharedPtr<SBox> ExpandedContainer;
	TSharedPtr<SBox> CollapsedPillContainer;
	TSharedPtr<SHorizontalBox> GraphicsRowBox;
	TSharedPtr<STextBlock> HeaderTitleBlock;
	TSharedPtr<STextBlock> HideHintBlock;
	TSharedPtr<STextBlock> CollapsedPillTextBlock;
	TArray<SVerticalBox::FSlot*> RowSlots;
	TArray<TSharedPtr<STextBlock>> KeyBlocks;
	TArray<TSharedPtr<STextBlock>> ActionBlocks;
};

// ============================================================================
// 2. TOP-RIGHT DIRECTOR OVERLOOK INTERCOM PANEL
// ============================================================================
class STopRightDirectorPanel : public SCompoundWidget
{
public:
	static constexpr float PadLeftPx = 13.0f;
	static constexpr float PadTopPx = 10.0f;
	static constexpr float PadRightPx = 15.0f;
	static constexpr float PadBottomPx = 9.0f;
	static constexpr float HorizontalPadTotalPx = PadLeftPx + PadRightPx;

	SLATE_BEGIN_ARGS(STopRightDirectorPanel)
	{
		_Visibility = EVisibility::HitTestInvisible;
	}
	SLATE_END_ARGS()

	void Construct(const FArguments& /*InArgs*/)
	{
		SetVisibility(EVisibility::HitTestInvisible);

		const FSlateFontInfo HeaderFont = FCoreStyle::GetDefaultFontStyle("Bold", 13);
		const FSlateFontInfo BadgeFont = FCoreStyle::GetDefaultFontStyle("Bold", 11);
		const FSlateFontInfo IntercomFont = FCoreStyle::GetDefaultFontStyle("Bold", 14);

		ChildSlot
		.Padding(FMargin(PadLeftPx, PadTopPx, PadRightPx, PadBottomPx))
		[
			SNew(SVerticalBox)
			.Visibility(EVisibility::HitTestInvisible)

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 5.0f)
			[
				SNew(SHorizontalBox)
				.Visibility(EVisibility::HitTestInvisible)

				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.VAlign(VAlign_Center)
				[
					SAssignNew(HeaderTextBlock, STextBlock)
					.Visibility(EVisibility::HitTestInvisible)
					.Font(HeaderFont)
					.ColorAndOpacity(FLinearColor(0.05f, 0.88f, 1.00f, 1.0f))
					.ShadowOffset(FVector2D(1.0f, 1.0f))
					.ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.88f))
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(8.0f, 0.0f, 0.0f, 0.0f)
				[
					SAssignNew(BadgeTextBlock, STextBlock)
					.Visibility(EVisibility::HitTestInvisible)
					.Font(BadgeFont)
					.ColorAndOpacity(FLinearColor(0.45f, 0.84f, 1.00f, 0.95f))
					.ShadowOffset(FVector2D(1.0f, 1.0f))
					.ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.82f))
				]
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 3.0f, 0.0f, 2.0f)
			[
				SAssignNew(IntercomBodyBlock, STextBlock)
				.Visibility(EVisibility::HitTestInvisible)
				.Font(IntercomFont)
				.AutoWrapText(true)
				.WrapTextAt(494.0f)
				.ShadowOffset(FVector2D(1.0f, 1.0f))
				.ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.88f))
			]
		];
	}

	void SetConfig(const FHudPresentationConfig& InCfg)
	{
		Config = InCfg;
		CachedHeaderPt = -1;
	}

	void UpdateResponsiveTypography(float EffectiveCornerWidth)
	{
		constexpr float ReferenceWidth = 525.0f;
		const bool bCompactScale = (EffectiveCornerWidth < 500.0f);
		const float FontScale = bCompactScale
			? FMath::Clamp(EffectiveCornerWidth / ReferenceWidth, 0.80f, 1.0f)
			: 1.0f;

		auto ScalePt = [bCompactScale, FontScale](int32 BasePt, int32 MinPt, int32 MaxPt) -> int32
		{
			const int32 RawPt = bCompactScale
				? FMath::FloorToInt(static_cast<float>(BasePt) * FontScale)
				: BasePt;
			return FMath::Clamp(RawPt, MinPt, MaxPt);
		};

		const int32 HeaderPt = ScalePt(Config.CornerHeaderFontSizePt, 9, 22);
		const int32 BodyPt = ScalePt(Config.CornerBodyFontSizePt, 10, 24);
		const int32 BadgePt = ScalePt(Config.CornerPillFontSizePt, 8, 20);

		if (HeaderPt != CachedHeaderPt || BodyPt != CachedBodyPt || BadgePt != CachedBadgePt)
		{
			CachedHeaderPt = HeaderPt;
			CachedBodyPt = BodyPt;
			CachedBadgePt = BadgePt;

			if (HeaderTextBlock.IsValid())
			{
				HeaderTextBlock->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", HeaderPt));
			}
			if (BadgeTextBlock.IsValid())
			{
				BadgeTextBlock->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", BadgePt));
			}
			if (IntercomBodyBlock.IsValid())
			{
				IntercomBodyBlock->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", BodyPt));
			}
		}

		SetWrapWidth(EffectiveCornerWidth - HorizontalPadTotalPx);
	}

	void SetWrapWidth(float InWrapWidth)
	{
		const float SafeWrap = FMath::Max(180.0f, InWrapWidth);
		if (!FMath::IsNearlyEqual(CachedWrapWidth, SafeWrap, 0.5f))
		{
			CachedWrapWidth = SafeWrap;
			if (IntercomBodyBlock.IsValid())
			{
				IntercomBodyBlock->SetWrapTextAt(SafeWrap);
			}
		}
	}

	float GetHeaderBottomLocalY() const
	{
		const float HeaderH = (HeaderTextBlock.IsValid() && HeaderTextBlock->GetDesiredSize().Y > 4.0f)
			? HeaderTextBlock->GetDesiredSize().Y
			: 19.0f;
		const float BadgeH = (BadgeTextBlock.IsValid() && BadgeTextBlock->GetDesiredSize().Y > 4.0f)
			? BadgeTextBlock->GetDesiredSize().Y
			: 0.0f;
		return PadTopPx + FMath::Max(HeaderH, BadgeH);
	}

	float GetDividerLocalY() const
	{
		return GetHeaderBottomLocalY() + 3.0f;
	}

	void ApplyViewModel(const FTopRightDirectorViewModel& VM)
	{
		AccentColor = VM.AccentBarColor;

		if (HeaderTextBlock.IsValid())
		{
			HeaderTextBlock->SetText(VM.HeaderText);
		}
		if (BadgeTextBlock.IsValid())
		{
			BadgeTextBlock->SetText(VM.StatusBadgeText);
			BadgeTextBlock->SetColorAndOpacity(VM.AccentBarColor);
		}
		if (IntercomBodyBlock.IsValid())
		{
			IntercomBodyBlock->SetText(VM.IntercomBodyText);
			IntercomBodyBlock->SetColorAndOpacity(VM.IntercomTextColor);
		}
	}

	virtual int32 OnPaint(
		const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override
	{
		const FVector2D Size = AllottedGeometry.GetLocalSize();
		const FLinearColor FillColor = TareTitheHudPaint::WithAlpha(Config.PanelAnthraciteTint, Config.CornerPanelOpacity);
		const FLinearColor BorderColor = Config.PanelBorderGunmetalTint;

		TareTitheHudPaint::DrawSolidBox(OutDrawElements, LayerId, AllottedGeometry, FVector2D::ZeroVector, Size, FillColor);
		TareTitheHudPaint::DrawPerimeterBorder(OutDrawElements, LayerId + 1, AllottedGeometry, FVector2D::ZeroVector, Size, 1.0f, BorderColor);
		TareTitheHudPaint::DrawSolidBox(
			OutDrawElements,
			LayerId + 2,
			AllottedGeometry,
			FVector2D(FMath::Max(0.0f, Size.X - 4.0f), 0.0f),
			FVector2D(4.0f, Size.Y),
			TareTitheHudPaint::WithAlpha(AccentColor, 0.95f));

		const float DividerY = GetDividerLocalY();
		TareTitheHudPaint::DrawSolidBox(
			OutDrawElements,
			LayerId + 1,
			AllottedGeometry,
			FVector2D(PadLeftPx - 2.0f, DividerY),
			FVector2D(FMath::Max(0.0f, Size.X - (HorizontalPadTotalPx - 4.0f)), 1.0f),
			FLinearColor(0.20f, 0.26f, 0.32f, 0.45f));

		return SCompoundWidget::OnPaint(
			Args,
			AllottedGeometry,
			MyCullingRect,
			OutDrawElements,
			LayerId + 3,
			InWidgetStyle,
			bParentEnabled);
	}

private:
	FHudPresentationConfig Config;
	FLinearColor AccentColor = FLinearColor(0.05f, 0.86f, 1.00f, 1.0f);
	float CachedWrapWidth = 494.0f;
	int32 CachedHeaderPt = -1;
	int32 CachedBodyPt = -1;
	int32 CachedBadgePt = -1;

	TSharedPtr<STextBlock> HeaderTextBlock;
	TSharedPtr<STextBlock> BadgeTextBlock;
	TSharedPtr<STextBlock> IntercomBodyBlock;
};

// ============================================================================
// 3. CENTER-SCREEN DYNAMIC FLASHING ALERT & TERMINAL VERDICT BANNER
// ============================================================================
class SCenterAlertBanner : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SCenterAlertBanner)
	{
		_Visibility = EVisibility::Collapsed;
	}
	SLATE_END_ARGS()

	void Construct(const FArguments& /*InArgs*/)
	{
		SetVisibility(EVisibility::Collapsed);

		PrimaryHazardFont = FCoreStyle::GetDefaultFontStyle("Bold", 12);
		PrimaryTutorialFont = FCoreStyle::GetDefaultFontStyle("Bold", 13);
		PrimaryStampFont = FCoreStyle::GetDefaultFontStyle("Bold", 15);
		SecondaryFont = FCoreStyle::GetDefaultFontStyle("Bold", 9);
		SecondaryTutorialFont = FCoreStyle::GetDefaultFontStyle("Bold", 10);

		ChildSlot
		.Padding(FMargin(18.0f, 8.0f, 18.0f, 8.0f))
		[
			SNew(SVerticalBox)
			.Visibility(EVisibility::HitTestInvisible)

			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Center)
			[
				SAssignNew(PrimaryTextBlock, STextBlock)
				.Visibility(EVisibility::HitTestInvisible)
				.Justification(ETextJustify::Center)
				.Font(PrimaryHazardFont)
				.ShadowOffset(FVector2D(1.0f, 1.0f))
				.ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.90f))
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Center)
			.Padding(0.0f, 3.0f, 0.0f, 0.0f)
			[
				SAssignNew(SecondaryTextBlock, STextBlock)
				.Visibility(EVisibility::HitTestInvisible)
				.Justification(ETextJustify::Center)
				.Font(SecondaryFont)
				.ShadowOffset(FVector2D(1.0f, 1.0f))
				.ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.85f))
			]
		];
	}

	void SetConfig(const FHudPresentationConfig& InCfg)
	{
		Config = InCfg;
	}

	void ApplyViewModel(const FCenterAlertViewModel& VM)
	{
		CurrentVM = VM;

		if (VM.AlertState == EHudCenterAlertState::None)
		{
			StopPulseTimerImmediate();
			CurrentBgOpacity = 0.0f;
			CurrentTextOpacity = 0.0f;
			CurrentBorderOpacity = 0.0f;
			SetVisibility(EVisibility::Collapsed);
			return;
		}

		SetVisibility(EVisibility::HitTestInvisible);

		if (PrimaryTextBlock.IsValid())
		{
			PrimaryTextBlock->SetFont(VM.bIsTerminalStamp
				? PrimaryStampFont
				: (VM.bIsTutorialHint ? PrimaryTutorialFont : PrimaryHazardFont));
			PrimaryTextBlock->SetText(VM.PrimaryBannerText);
		}

		if (SecondaryTextBlock.IsValid())
		{
			SecondaryTextBlock->SetFont(VM.bIsTutorialHint ? SecondaryTutorialFont : SecondaryFont);
			if (!VM.SecondarySubText.IsEmpty())
			{
				SecondaryTextBlock->SetText(VM.SecondarySubText);
				SecondaryTextBlock->SetVisibility(EVisibility::HitTestInvisible);
			}
			else
			{
				SecondaryTextBlock->SetText(FText::GetEmpty());
				SecondaryTextBlock->SetVisibility(EVisibility::Collapsed);
			}
		}

		if (VM.bIsTerminalStamp)
		{
			StopPulseTimerImmediate();
			CurrentBgOpacity = 0.92f;
			CurrentTextOpacity = 1.0f;
			CurrentBorderOpacity = 1.0f;
			ApplyCurrentTextColors();
			Invalidate(EInvalidateWidgetReason::Paint);
			return;
		}

		// P1 / P2 rising-edge 3-pulse (1.05s) -> steady 0.62f bg; P3 tutorial 2-pulse (0.80s) -> steady 0.76f bg at 0.00 ms idle cost.
		if (VM.TriggerGeneration != LastTriggerGeneration)
		{
			LastTriggerGeneration = VM.TriggerGeneration;
			PulseElapsedSeconds = 0.0f;
			CurrentBgOpacity = VM.bIsTutorialHint ? 0.84f : 0.90f;
			CurrentTextOpacity = 1.0f;
			CurrentBorderOpacity = 1.0f;

			if (!ActivePulseTimerHandle.IsValid())
			{
				ActivePulseTimerHandle = RegisterActiveTimer(
					0.0f,
					FWidgetActiveTimerDelegate::CreateSP(this, &SCenterAlertBanner::HandlePulseAnimationTick));
				bPulseTimerRunning = true;
			}
		}

		ApplyCurrentTextColors();
		Invalidate(EInvalidateWidgetReason::Paint);
	}

	bool IsPulseTimerActive() const { return bPulseTimerRunning; }
	float GetCurrentBgOpacity() const { return CurrentBgOpacity; }
	float GetCurrentTextOpacity() const { return CurrentTextOpacity; }

	virtual int32 OnPaint(
		const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override
	{
		const FVector2D Size = AllottedGeometry.GetLocalSize();
		const FLinearColor BaseAnthracite = CurrentVM.bIsTerminalStamp
			? FLinearColor(0.010f, 0.012f, 0.016f, CurrentBgOpacity)
			: TareTitheHudPaint::WithAlpha(Config.PanelAnthraciteTint, CurrentBgOpacity);

		TareTitheHudPaint::DrawSolidBox(OutDrawElements, LayerId, AllottedGeometry, FVector2D::ZeroVector, Size, BaseAnthracite);

		const float OuterThickness = CurrentVM.bIsTerminalStamp ? 2.0f : 1.5f;
		const FLinearColor AccentBorder = TareTitheHudPaint::WithAlpha(CurrentVM.AccentColor, CurrentBorderOpacity);
		TareTitheHudPaint::DrawPerimeterBorder(
			OutDrawElements,
			LayerId + 1,
			AllottedGeometry,
			FVector2D::ZeroVector,
			Size,
			OuterThickness,
			AccentBorder);

		if (CurrentVM.bIsTerminalStamp)
		{
			TareTitheHudPaint::DrawPerimeterBorder(
				OutDrawElements,
				LayerId + 1,
				AllottedGeometry,
				FVector2D(4.0f, 4.0f),
				Size - FVector2D(8.0f, 8.0f),
				1.0f,
				TareTitheHudPaint::WithAlpha(CurrentVM.AccentColor, 0.42f * CurrentBorderOpacity));
		}

		const float CapWidth = CurrentVM.bIsTerminalStamp ? 5.0f : 4.0f;
		TareTitheHudPaint::DrawSolidBox(OutDrawElements, LayerId + 2, AllottedGeometry, FVector2D(0.0f, 0.0f), FVector2D(CapWidth, Size.Y), AccentBorder);
		TareTitheHudPaint::DrawSolidBox(OutDrawElements, LayerId + 2, AllottedGeometry, FVector2D(FMath::Max(0.0f, Size.X - CapWidth), 0.0f), FVector2D(CapWidth, Size.Y), AccentBorder);

		return SCompoundWidget::OnPaint(
			Args,
			AllottedGeometry,
			MyCullingRect,
			OutDrawElements,
			LayerId + 3,
			InWidgetStyle,
			bParentEnabled);
	}

private:
	void StopPulseTimerImmediate()
	{
		if (ActivePulseTimerHandle.IsValid())
		{
			if (TSharedPtr<FActiveTimerHandle> PinnedHandle = ActivePulseTimerHandle.Pin())
			{
				UnRegisterActiveTimer(PinnedHandle.ToSharedRef());
			}
			ActivePulseTimerHandle.Reset();
		}
		bPulseTimerRunning = false;
	}

	void ApplyCurrentTextColors()
	{
		if (PrimaryTextBlock.IsValid())
		{
			PrimaryTextBlock->SetColorAndOpacity(TareTitheHudPaint::WithAlpha(CurrentVM.AccentColor, CurrentTextOpacity));
		}
		if (SecondaryTextBlock.IsValid())
		{
			const FLinearColor SubColor(0.88f, 0.92f, 0.96f, CurrentTextOpacity * 0.92f);
			SecondaryTextBlock->SetColorAndOpacity(SubColor);
		}
	}

	EActiveTimerReturnType HandlePulseAnimationTick(double /*InCurrentTime*/, float InDeltaTime)
	{
		if (CurrentVM.AlertState == EHudCenterAlertState::None || CurrentVM.bIsTerminalStamp)
		{
			ActivePulseTimerHandle.Reset();
			bPulseTimerRunning = false;
			return EActiveTimerReturnType::Stop;
		}

		PulseElapsedSeconds += FMath::Max(0.0f, InDeltaTime);
		const bool bTutorial = CurrentVM.bIsTutorialHint;
		const float TotalEnvelopeSeconds = bTutorial
			? FMath::Max(0.20f, Config.TutorialPulseEnvelopeSeconds)
			: FMath::Max(0.25f, Config.AlertPulseEnvelopeSeconds);

		if (PulseElapsedSeconds >= TotalEnvelopeSeconds)
		{
			CurrentBgOpacity = bTutorial ? Config.TutorialPostPulseBackgroundOpacity : Config.AlertPostPulseBackgroundOpacity;
			CurrentTextOpacity = bTutorial ? 0.98f : Config.AlertPostPulseTextOpacity;
			CurrentBorderOpacity = bTutorial ? 0.90f : 0.85f;
			ApplyCurrentTextColors();
			Invalidate(EInvalidateWidgetReason::Paint);
			ActivePulseTimerHandle.Reset();
			bPulseTimerRunning = false;
			return EActiveTimerReturnType::Stop;
		}

		const float NormT = PulseElapsedSeconds / TotalEnvelopeSeconds;
		const float PulseCount = static_cast<float>(FMath::Max(1, bTutorial ? Config.TutorialPulseCount : Config.AlertPulseCount));
		const float PulseWave = 0.5f + 0.5f * FMath::Cos(NormT * PulseCount * 2.0f * UE_PI);

		if (bTutorial)
		{
			CurrentBgOpacity = FMath::Lerp(0.58f, 0.84f, PulseWave);
			CurrentTextOpacity = FMath::Lerp(0.82f, 1.00f, PulseWave);
			CurrentBorderOpacity = FMath::Lerp(0.62f, 1.00f, PulseWave);
		}
		else
		{
			CurrentBgOpacity = FMath::Lerp(0.32f, 0.90f, PulseWave);
			CurrentTextOpacity = FMath::Lerp(0.68f, 1.00f, PulseWave);
			CurrentBorderOpacity = FMath::Lerp(0.42f, 1.00f, PulseWave);
		}
		ApplyCurrentTextColors();
		Invalidate(EInvalidateWidgetReason::Paint);
		return EActiveTimerReturnType::Continue;
	}

	FHudPresentationConfig Config;
	FCenterAlertViewModel CurrentVM;
	uint64 LastTriggerGeneration = 0;
	float PulseElapsedSeconds = 0.0f;
	float CurrentBgOpacity = 0.0f;
	float CurrentTextOpacity = 0.0f;
	float CurrentBorderOpacity = 0.0f;
	bool bPulseTimerRunning = false;

	TWeakPtr<FActiveTimerHandle> ActivePulseTimerHandle;
	FSlateFontInfo PrimaryHazardFont;
	FSlateFontInfo PrimaryTutorialFont;
	FSlateFontInfo PrimaryStampFont;
	FSlateFontInfo SecondaryFont;
	FSlateFontInfo SecondaryTutorialFont;
	TSharedPtr<STextBlock> PrimaryTextBlock;
	TSharedPtr<STextBlock> SecondaryTextBlock;
};

// ============================================================================
// 4. PROJECTED RETICLE READOUT CARD (~0.65 DARK BACKING BAND)
// ============================================================================
class SReticleReadoutCard : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SReticleReadoutCard)
	{
		_Visibility = EVisibility::HitTestInvisible;
	}
	SLATE_END_ARGS()

	void Construct(const FArguments& /*InArgs*/)
	{
		SetVisibility(EVisibility::HitTestInvisible);

		const FSlateFontInfo PrimaryFont = FCoreStyle::GetDefaultFontStyle("Bold", 10);
		const FSlateFontInfo YieldFont = FCoreStyle::GetDefaultFontStyle("Bold", 9);
		const FSlateFontInfo LoadFont = FCoreStyle::GetDefaultFontStyle("Regular", 9);

		ChildSlot
		.Padding(FMargin(10.0f, 6.0f, 10.0f, 6.0f))
		[
			SNew(SVerticalBox)
			.Visibility(EVisibility::HitTestInvisible)

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 2.0f)
			[
				SAssignNew(PrimaryTextBlock, STextBlock)
				.Visibility(EVisibility::HitTestInvisible)
				.Font(PrimaryFont)
				.ShadowOffset(FVector2D(1.0f, 1.0f))
				.ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.85f))
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 2.0f)
			[
				SAssignNew(YieldTextBlock, STextBlock)
				.Visibility(EVisibility::HitTestInvisible)
				.Font(YieldFont)
				.ShadowOffset(FVector2D(1.0f, 1.0f))
				.ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.85f))
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SAssignNew(ClampLoadTextBlock, STextBlock)
				.Visibility(EVisibility::HitTestInvisible)
				.Font(LoadFont)
				.ShadowOffset(FVector2D(1.0f, 1.0f))
				.ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.85f))
			]
		];
	}

	void SetConfig(const FHudPresentationConfig& InCfg)
	{
		Config = InCfg;
	}

	void ApplyViewModel(const FReticleReadoutViewModel& VM)
	{
		if (!VM.bIsVisible)
		{
			SetVisibility(EVisibility::Collapsed);
			return;
		}

		SetVisibility(EVisibility::HitTestInvisible);
		AccentColor = VM.AccentColor;

		if (PrimaryTextBlock.IsValid())
		{
			PrimaryTextBlock->SetText(VM.PrimaryNeedOrStatusText);
			PrimaryTextBlock->SetColorAndOpacity(VM.PrimaryColor);
		}
		if (YieldTextBlock.IsValid())
		{
			YieldTextBlock->SetText(VM.YieldEstimateText);
			YieldTextBlock->SetColorAndOpacity(VM.YieldColor);
		}
		if (ClampLoadTextBlock.IsValid())
		{
			ClampLoadTextBlock->SetText(VM.ClampLoadText);
			ClampLoadTextBlock->SetColorAndOpacity(VM.ClampLoadColor);
		}
	}

	virtual int32 OnPaint(
		const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override
	{
		const FVector2D Size = AllottedGeometry.GetLocalSize();
		const FLinearColor BackingColor(0.010f, 0.013f, 0.018f, Config.ReticleBackingOpacity);
		const FLinearColor BorderColor = TareTitheHudPaint::WithAlpha(Config.PanelBorderGunmetalTint, 0.72f);

		TareTitheHudPaint::DrawSolidBox(OutDrawElements, LayerId, AllottedGeometry, FVector2D::ZeroVector, Size, BackingColor);
		TareTitheHudPaint::DrawPerimeterBorder(OutDrawElements, LayerId + 1, AllottedGeometry, FVector2D::ZeroVector, Size, 1.0f, BorderColor);
		TareTitheHudPaint::DrawSolidBox(
			OutDrawElements,
			LayerId + 2,
			AllottedGeometry,
			FVector2D(0.0f, 0.0f),
			FVector2D(3.0f, Size.Y),
			TareTitheHudPaint::WithAlpha(AccentColor, 0.92f));

		return SCompoundWidget::OnPaint(
			Args,
			AllottedGeometry,
			MyCullingRect,
			OutDrawElements,
			LayerId + 3,
			InWidgetStyle,
			bParentEnabled);
	}

private:
	FHudPresentationConfig Config;
	FLinearColor AccentColor = FLinearColor(0.05f, 0.90f, 1.00f, 1.0f);

	TSharedPtr<STextBlock> PrimaryTextBlock;
	TSharedPtr<STextBlock> YieldTextBlock;
	TSharedPtr<STextBlock> ClampLoadTextBlock;
};

// ============================================================================
// 5. FRAMED BOTTOM CRT WEIGHING CONSOLE (PROCEDURAL SCANLINES + ANALOG BAR)
// ============================================================================
class SBottomCrtWeighingConsole : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SBottomCrtWeighingConsole)
	{
		_Visibility = EVisibility::HitTestInvisible;
	}
	SLATE_END_ARGS()

	void Construct(const FArguments& /*InArgs*/)
	{
		SetVisibility(EVisibility::HitTestInvisible);

		const FSlateFontInfo HeaderFont = FCoreStyle::GetDefaultFontStyle("Bold", 10);
		const FSlateFontInfo WindowFont = FCoreStyle::GetDefaultFontStyle("Regular", 9);
		const FSlateFontInfo MassHeroFont = FCoreStyle::GetDefaultFontStyle("Bold", 17);
		const FSlateFontInfo BadgeFont = FCoreStyle::GetDefaultFontStyle("Bold", 9);
		const FSlateFontInfo StatusFont = FCoreStyle::GetDefaultFontStyle("Bold", 10);

		ChildSlot
		.Padding(FMargin(14.0f, 8.0f, 14.0f, 7.0f))
		[
			SNew(SVerticalBox)
			.Visibility(EVisibility::HitTestInvisible)

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 3.0f)
			[
				SNew(SHorizontalBox)
				.Visibility(EVisibility::HitTestInvisible)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SAssignNew(ConsoleHeaderBlock, STextBlock)
					.Visibility(EVisibility::HitTestInvisible)
					.Font(HeaderFont)
					.ColorAndOpacity(FLinearColor(0.05f, 0.88f, 1.00f, 1.0f))
				]

				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SNew(SSpacer)
					.Visibility(EVisibility::HitTestInvisible)
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SAssignNew(WindowsSummaryBlock, STextBlock)
					.Visibility(EVisibility::HitTestInvisible)
					.Font(WindowFont)
					.ColorAndOpacity(FLinearColor(0.76f, 0.88f, 0.96f, 0.95f))
				]
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 2.0f, 0.0f, 6.0f)
			[
				SNew(SHorizontalBox)
				.Visibility(EVisibility::HitTestInvisible)

				+ SHorizontalBox::Slot()
				.FillWidth(0.25f)
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				[
					SAssignNew(TareBadgeBlock, STextBlock)
					.Visibility(EVisibility::HitTestInvisible)
					.Font(BadgeFont)
				]

				+ SHorizontalBox::Slot()
				.FillWidth(0.50f)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					SNew(SHorizontalBox)
					.Visibility(EVisibility::HitTestInvisible)

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SAssignNew(MassReadoutBlock, STextBlock)
						.Visibility(EVisibility::HitTestInvisible)
						.Justification(ETextJustify::Center)
						.Font(MassHeroFont)
						.ShadowOffset(FVector2D(1.0f, 1.0f))
						.ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.92f))
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(7.0f, 0.0f, 0.0f, 0.0f)
					[
						SAssignNew(MassStatusBadgeBlock, STextBlock)
						.Visibility(EVisibility::HitTestInvisible)
						.Font(StatusFont)
						.ShadowOffset(FVector2D(1.0f, 1.0f))
						.ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.90f))
					]
				]

				+ SHorizontalBox::Slot()
				.FillWidth(0.25f)
				.HAlign(HAlign_Right)
				.VAlign(VAlign_Center)
				[
					SAssignNew(RouteBadgeBlock, STextBlock)
					.Visibility(EVisibility::HitTestInvisible)
					.Justification(ETextJustify::Right)
					.Font(BadgeFont)
				]
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 3.0f)
			[
				SAssignNew(AnalogBarSpacerBox, SBox)
				.Visibility(EVisibility::HitTestInvisible)
				.HeightOverride(7.0f)
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Center)
			.Padding(0.0f, 1.0f, 0.0f, 1.0f)
			[
				SAssignNew(OperationalStatusBlock, STextBlock)
				.Visibility(EVisibility::HitTestInvisible)
				.Justification(ETextJustify::Center)
				.Font(StatusFont)
			]
		];
	}

	void SetConfig(const FHudPresentationConfig& InCfg)
	{
		Config = InCfg;
	}

	void ApplyViewModel(const FBottomCrtConsoleViewModel& VM)
	{
		CurrentVM = VM;

		if (ConsoleHeaderBlock.IsValid())
		{
			ConsoleHeaderBlock->SetText(VM.ConsoleHeaderText);
		}
		if (WindowsSummaryBlock.IsValid())
		{
			WindowsSummaryBlock->SetText(VM.WindowsSummaryText);
		}
		if (MassReadoutBlock.IsValid())
		{
			MassReadoutBlock->SetText(VM.MassReadoutText);
			MassReadoutBlock->SetColorAndOpacity(VM.MassReadoutColor);
		}
		if (MassStatusBadgeBlock.IsValid())
		{
			const bool bVisible = VM.bShowMassStatusBadge && !VM.MassStatusBadgeText.IsEmpty();
			MassStatusBadgeBlock->SetVisibility(bVisible ? EVisibility::HitTestInvisible : EVisibility::Collapsed);
			if (bVisible)
			{
				MassStatusBadgeBlock->SetText(VM.MassStatusBadgeText);
				MassStatusBadgeBlock->SetColorAndOpacity(VM.MassStatusBadgeColor);
			}
		}
		if (TareBadgeBlock.IsValid())
		{
			TareBadgeBlock->SetText(VM.TareBadgeText);
			TareBadgeBlock->SetColorAndOpacity(VM.TareBadgeColor);
		}
		if (RouteBadgeBlock.IsValid())
		{
			RouteBadgeBlock->SetText(VM.RouteBadgeText);
			RouteBadgeBlock->SetColorAndOpacity(VM.RouteBadgeColor);
		}
		if (OperationalStatusBlock.IsValid())
		{
			OperationalStatusBlock->SetText(VM.OperationalStatusText);
			OperationalStatusBlock->SetColorAndOpacity(VM.OperationalStatusColor);
		}
	}

	float GetLastMassReadoutBottomLocalY() const { return LastMassReadoutBottomLocalY; }
	float GetLastAnalogBarTopLocalY() const { return LastAnalogBarTopLocalY; }

	virtual int32 OnPaint(
		const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override
	{
		const FVector2D Size = AllottedGeometry.GetLocalSize();

		const FLinearColor OuterHousingTint(0.032f, 0.038f, 0.046f, 0.94f);
		TareTitheHudPaint::DrawSolidBox(OutDrawElements, LayerId, AllottedGeometry, FVector2D::ZeroVector, Size, OuterHousingTint);
		TareTitheHudPaint::DrawPerimeterBorder(OutDrawElements, LayerId + 1, AllottedGeometry, FVector2D::ZeroVector, Size, 1.5f, Config.PanelBorderGunmetalTint);

		const FVector2D WellInset(4.0f, 4.0f);
		const FVector2D WellSize = FVector2D(FMath::Max(0.0f, Size.X - 8.0f), FMath::Max(0.0f, Size.Y - 8.0f));
		const FLinearColor CrtWellFill(0.007f, 0.012f, 0.016f, Config.BottomCrtConsoleOpacity);
		TareTitheHudPaint::DrawSolidBox(OutDrawElements, LayerId + 1, AllottedGeometry, WellInset, WellSize, CrtWellFill);
		TareTitheHudPaint::DrawPerimeterBorder(
			OutDrawElements,
			LayerId + 2,
			AllottedGeometry,
			WellInset,
			WellSize,
			1.0f,
			FLinearColor(0.06f, 0.32f, 0.40f, 0.55f));

		// Correction E: Discrete 1px-tall WhiteBrush MakeBox strips at CrtScanlinePitchPx intervals (zero diagonal polyline zig-zags).
		const float ScanlineAlpha = FMath::Clamp(Config.CrtScanlineOpacity, 0.0f, 0.60f);
		const float PitchPx = FMath::Max(2.0f, Config.CrtScanlinePitchPx);
		if (ScanlineAlpha > KINDA_SMALL_NUMBER && WellSize.X > 2.0f && WellSize.Y > 2.0f)
		{
			const FLinearColor ScanlineColor(0.0f, 0.0f, 0.0f, ScanlineAlpha);
			const float StartY = WellInset.Y + 1.0f;
			const float EndY = WellInset.Y + WellSize.Y - 1.0f;
			for (float RowY = StartY; RowY < EndY; RowY += PitchPx)
			{
				TareTitheHudPaint::DrawSolidBox(
					OutDrawElements,
					LayerId + 2,
					AllottedGeometry,
					FVector2D(WellInset.X + 1.0f, RowY),
					FVector2D(WellSize.X - 2.0f, 1.0f),
					ScanlineColor);
			}
		}

		TareTitheHudPaint::DrawCornerBrackets(
			OutDrawElements,
			LayerId + 3,
			AllottedGeometry,
			Size,
			10.0f,
			2.0f,
			FLinearColor(0.08f, 0.78f, 0.92f, 0.75f));

		const int32 OutLayerId = SCompoundWidget::OnPaint(
			Args,
			AllottedGeometry,
			MyCullingRect,
			OutDrawElements,
			LayerId + 3,
			InWidgetStyle,
			bParentEnabled);

		if (MassReadoutBlock.IsValid())
		{
			const FGeometry& MassGeom = MassReadoutBlock->GetPaintSpaceGeometry();
			if (MassGeom.GetLocalSize().Y > 0.0f)
			{
				LastMassReadoutBottomLocalY = AllottedGeometry.AbsoluteToLocal(
					MassGeom.GetAbsolutePositionAtCoordinates(FVector2D(0.0f, 1.0f))).Y;
			}
		}

		const float BarLeftX = 16.0f;
		const float BarWidth = FMath::Max(0.0f, Size.X - 32.0f);
		float BarTopY = FMath::Max(38.0f, Size.Y - 31.0f);
		if (AnalogBarSpacerBox.IsValid())
		{
			const FGeometry& SpacerGeom = AnalogBarSpacerBox->GetPaintSpaceGeometry();
			if (SpacerGeom.GetLocalSize().Y > 0.0f)
			{
				BarTopY = AllottedGeometry.AbsoluteToLocal(SpacerGeom.GetAbsolutePosition()).Y + 1.0f;
			}
		}
		LastAnalogBarTopLocalY = BarTopY;
		const float BarHeight = 5.0f;
		const int32 BarLayerId = OutLayerId + 1;
		if (BarWidth > 20.0f)
		{
			TareTitheHudPaint::DrawSolidBox(
				OutDrawElements,
				BarLayerId,
				AllottedGeometry,
				FVector2D(BarLeftX, BarTopY),
				FVector2D(BarWidth, BarHeight),
				FLinearColor(0.04f, 0.06f, 0.08f, 0.90f));

			const float ValidMinX = BarLeftX + BarWidth * FMath::Clamp(CurrentVM.NormalizedValidMin, 0.0f, 1.0f);
			const float ValidMaxX = BarLeftX + BarWidth * FMath::Clamp(CurrentVM.NormalizedValidMax, 0.0f, 1.0f);
			if (ValidMaxX > ValidMinX)
			{
				TareTitheHudPaint::DrawSolidBox(
					OutDrawElements,
					BarLayerId,
					AllottedGeometry,
					FVector2D(ValidMinX, BarTopY - 1.0f),
					FVector2D(ValidMaxX - ValidMinX, BarHeight + 2.0f),
					FLinearColor(0.04f, 0.72f, 0.32f, 0.36f));
			}

			const float DirMinX = BarLeftX + BarWidth * FMath::Clamp(CurrentVM.NormalizedDirCutMin, 0.0f, 1.0f);
			const float DirMaxX = BarLeftX + BarWidth * FMath::Clamp(CurrentVM.NormalizedDirCutMax, 0.0f, 1.0f);
			if (DirMaxX > DirMinX)
			{
				TareTitheHudPaint::DrawSolidBox(
					OutDrawElements,
					BarLayerId,
					AllottedGeometry,
					FVector2D(DirMinX, BarTopY - 1.0f),
					FVector2D(DirMaxX - DirMinX, BarHeight + 2.0f),
					FLinearColor(1.00f, 0.80f, 0.14f, 0.50f));
			}

			const float FillW = BarWidth * FMath::Clamp(CurrentVM.NormalizedMassFill, 0.0f, 1.0f);
			if (FillW > 0.5f)
			{
				TareTitheHudPaint::DrawSolidBox(
					OutDrawElements,
					BarLayerId + 1,
					AllottedGeometry,
					FVector2D(BarLeftX, BarTopY),
					FVector2D(FillW, BarHeight),
					TareTitheHudPaint::WithAlpha(CurrentVM.MassReadoutColor, 0.92f));
			}
		}

		return BarLayerId + 2;
	}

private:
	FHudPresentationConfig Config;
	FBottomCrtConsoleViewModel CurrentVM;
	mutable float LastMassReadoutBottomLocalY = 0.0f;
	mutable float LastAnalogBarTopLocalY = 0.0f;

	TSharedPtr<SBox> AnalogBarSpacerBox;
	TSharedPtr<STextBlock> ConsoleHeaderBlock;
	TSharedPtr<STextBlock> WindowsSummaryBlock;
	TSharedPtr<STextBlock> MassReadoutBlock;
	TSharedPtr<STextBlock> MassStatusBadgeBlock;
	TSharedPtr<STextBlock> TareBadgeBlock;
	TSharedPtr<STextBlock> RouteBadgeBlock;
	TSharedPtr<STextBlock> OperationalStatusBlock;
};

// ============================================================================
// ROOT OVERLAY CONTAINER (STareTitheHudOverlay)
// ============================================================================
STareTitheHudOverlay::STareTitheHudOverlay()
{
	SetCanTick(false);
}

STareTitheHudOverlay::~STareTitheHudOverlay() = default;

void STareTitheHudOverlay::Construct(const FArguments& InArgs)
{
	ActiveConfig = InArgs._HudConfig;
	EffectiveLocalViewportSize = FVector2D(
		FMath::Max(320.0f, InArgs._InitialLocalViewportSize.X),
		FMath::Max(240.0f, InArgs._InitialLocalViewportSize.Y));
	CachedUnoverriddenViewportSize = EffectiveLocalViewportSize;
	SetVisibility(EVisibility::HitTestInvisible);

	SAssignNew(TopLeftPanel, STopLeftIndenturePanel);
	SAssignNew(LeftControlsCard, SLeftControlsCard);
	SAssignNew(TopRightPanel, STopRightDirectorPanel);
	SAssignNew(CenterAlertBanner, SCenterAlertBanner);
	SAssignNew(ReticleReadoutCard, SReticleReadoutCard);
	SAssignNew(BottomCrtConsole, SBottomCrtWeighingConsole);

	ChildSlot
	[
		SAssignNew(ViewportTestOverrideBox, SBox)
		.Visibility(EVisibility::HitTestInvisible)
		[
			SAssignNew(RootOverlay, SOverlay)
			.Visibility(EVisibility::HitTestInvisible)

		+ SOverlay::Slot()
		.Expose(TopLeftOverlaySlot)
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Top)
		[
			SNew(SVerticalBox)
			.Visibility(EVisibility::HitTestInvisible)

			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Left)
			[
				SAssignNew(TopLeftWidthBox, SBox)
				.Visibility(EVisibility::HitTestInvisible)
				[
					TopLeftPanel.ToSharedRef()
				]
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Left)
			.Padding(0.0f, 8.0f, 0.0f, 0.0f)
			[
				SAssignNew(LeftControlsWidthBox, SBox)
				.Visibility(EVisibility::HitTestInvisible)
				[
					LeftControlsCard.ToSharedRef()
				]
			]
		]

		+ SOverlay::Slot()
		.Expose(TopRightOverlaySlot)
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Top)
		[
			SAssignNew(TopRightWidthBox, SBox)
			.Visibility(EVisibility::HitTestInvisible)
			[
				TopRightPanel.ToSharedRef()
			]
		]

		+ SOverlay::Slot()
		.Expose(BottomConsoleOverlaySlot)
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Bottom)
		.Padding(0.0f, 0.0f, 0.0f, 12.0f)
		[
			SAssignNew(BottomConsoleWidthBox, SBox)
			.Visibility(EVisibility::HitTestInvisible)
			[
				BottomCrtConsole.ToSharedRef()
			]
		]

		+ SOverlay::Slot()
		.Expose(ReticleOverlaySlot)
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Top)
		[
			ReticleReadoutCard.ToSharedRef()
		]

		+ SOverlay::Slot()
		.Expose(CenterBannerOverlaySlot)
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Top)
		[
			SAssignNew(CenterBannerWidthBox, SBox)
			.Visibility(EVisibility::HitTestInvisible)
			.MinDesiredWidth(300.0f)
			[
				CenterAlertBanner.ToSharedRef()
			]
		]
		]
	];

	ApplyHudConfig(ActiveConfig);
}

void STareTitheHudOverlay::RefreshResponsiveMetrics()
{
	const float LocalW = FMath::Max(320.0f, EffectiveLocalViewportSize.X);
	const float Margin = FMath::Clamp(ActiveConfig.CornerPanelMarginPx, 6.0f, 40.0f);
	const float MaxCornerFrac = FMath::Clamp(ActiveConfig.CornerPanelMaxViewportWidthFraction, 0.14f, 0.38f);

	// Phase 12-FIX2: +50% corner expansion (525 px target, clamped to LocalW * 0.34f -> ~522.2 px at 125% DPI, 435.2 px at 720p).
	const float SafeMinFloor = FMath::Min(250.0f, LocalW * MaxCornerFrac);
	CachedEffectiveCornerWidth = FMath::Clamp(
		FMath::Min(ActiveConfig.CornerPanelWidthPx, LocalW * MaxCornerFrac),
		SafeMinFloor,
		ActiveConfig.CornerPanelWidthPx);

	if (TopLeftWidthBox.IsValid())
	{
		TopLeftWidthBox->SetWidthOverride(CachedEffectiveCornerWidth);
	}
	if (TopRightWidthBox.IsValid())
	{
		TopRightWidthBox->SetWidthOverride(CachedEffectiveCornerWidth);
	}
	if (TopLeftPanel.IsValid())
	{
		TopLeftPanel->UpdateResponsiveTypography(CachedEffectiveCornerWidth);
	}
	if (TopRightPanel.IsValid())
	{
		TopRightPanel->UpdateResponsiveTypography(CachedEffectiveCornerWidth);
	}

	// Phase 12-FIX5 Refinement 1: Target 326 px at 1080p with strict U_right <= 0.180 across 100%/125%/150% DPI
	// (0.0 px contact with Left Panopticon bezel even at max +4.5 deg camera yaw where U_window_left = 0.181).
	const float MaxAllowedControlsWidthForU = FMath::FloorToFloat((LocalW * 0.180f) - Margin - 0.5f);
	CachedEffectiveControlsWidth = FMath::Clamp(
		FMath::Min(326.0f, MaxAllowedControlsWidthForU),
		FMath::Min(132.0f, MaxAllowedControlsWidthForU),
		332.0f);

	if (LeftControlsCard.IsValid())
	{
		LeftControlsCard->UpdateResponsiveTypography(CachedEffectiveControlsWidth);
	}
	if (LeftControlsWidthBox.IsValid())
	{
		if (bIsControlsCardExpanded)
		{
			LeftControlsWidthBox->SetWidthOverride(CachedEffectiveControlsWidth);
		}
		else
		{
			LeftControlsWidthBox->SetWidthOverride(FOptionalSize());
		}
	}
	if (TopLeftOverlaySlot != nullptr)
	{
		TopLeftOverlaySlot->SetPadding(FMargin(Margin, Margin, 0.0f, 0.0f));
	}
	if (TopRightOverlaySlot != nullptr)
	{
		TopRightOverlaySlot->SetPadding(FMargin(0.0f, Margin, Margin, 0.0f));
	}

	// Correction B: Clamp BottomConsoleWidthPx (default 720.0f) to FMath::Min(HudConfig.BottomConsoleWidthPx, LocalViewportSize.X * 0.54f).
	const float MaxBottomFrac = FMath::Clamp(ActiveConfig.BottomConsoleMaxViewportWidthFraction, 0.35f, 0.75f);
	CachedEffectiveBottomWidth = FMath::Clamp(
		FMath::Min(ActiveConfig.BottomConsoleWidthPx, LocalW * MaxBottomFrac),
		FMath::Min(380.0f, LocalW * MaxBottomFrac),
		ActiveConfig.BottomConsoleWidthPx);
	if (BottomConsoleWidthBox.IsValid())
	{
		BottomConsoleWidthBox->SetWidthOverride(CachedEffectiveBottomWidth);
	}

	if (CenterBannerWidthBox.IsValid())
	{
		CenterBannerWidthBox->SetMaxDesiredWidth(FMath::Clamp(LocalW * 0.58f, 360.0f, 740.0f));
	}

	RefreshCenterBannerOffset();
	RefreshReticleClampedOffset();
}

void STareTitheHudOverlay::RefreshCenterBannerOffset()
{
	if (CenterBannerOverlaySlot == nullptr)
	{
		return;
	}

	const float LocalH = FMath::Max(240.0f, EffectiveLocalViewportSize.Y);
	const float Margin = FMath::Clamp(ActiveConfig.CornerPanelMarginPx, 6.0f, 40.0f);
	const float EstBottomH = (BottomCrtConsole.IsValid() && BottomCrtConsole->GetDesiredSize().Y > 10.0f)
		? BottomCrtConsole->GetDesiredSize().Y
		: 108.0f;
	const float EstBannerH = (CenterAlertBanner.IsValid() && CenterAlertBanner->GetDesiredSize().Y > 10.0f)
		? CenterAlertBanner->GetDesiredSize().Y
		: (bIsCenterBannerTerminalStamp ? 64.0f : 48.0f);

	const float AnchorNormY = bIsCenterBannerTerminalStamp
		? ActiveConfig.VerdictStampTopAnchorNormalized
		: ActiveConfig.HazardBannerTopAnchorNormalized;
	const float BottomTopY = LocalH - 12.0f - EstBottomH;
	const float BannerTopY = FMath::Clamp(
		AnchorNormY * LocalH,
		Margin,
		FMath::Max(Margin, BottomTopY - EstBannerH - 8.0f));

	CenterBannerOverlaySlot->SetPadding(FMargin(0.0f, BannerTopY, 0.0f, 0.0f));
}

void STareTitheHudOverlay::RefreshReticleClampedOffset()
{
	if (ReticleOverlaySlot == nullptr)
	{
		return;
	}

	const FVector2D LocalSize(
		FMath::Max(320.0f, EffectiveLocalViewportSize.X),
		FMath::Max(240.0f, EffectiveLocalViewportSize.Y));
	const float Margin = FMath::Clamp(ActiveConfig.CornerPanelMarginPx, 6.0f, 40.0f);

	const FVector2D RawDesired = ReticleReadoutCard.IsValid() ? ReticleReadoutCard->GetDesiredSize() : FVector2D::ZeroVector;
	const float SafeW = (RawDesired.X > 20.0f) ? RawDesired.X : 230.0f;
	const float SafeH = (RawDesired.Y > 15.0f) ? RawDesired.Y : 64.0f;

	// Correction C: Convert NormalizedUV * LocalSize inside Slate and clamp clear of Top-Right, Center Banner, and Bottom CRT Console.
	FVector2D CardPos = CachedReticleNormalizedUV * LocalSize;
	CardPos.Y -= 0.5f * SafeH;

	CardPos.X = FMath::Clamp(CardPos.X, Margin, FMath::Max(Margin, LocalSize.X - Margin - SafeW));
	CardPos.Y = FMath::Clamp(CardPos.Y, Margin, FMath::Max(Margin, LocalSize.Y - Margin - SafeH));

	const float TopLeftRightX = Margin + CachedEffectiveCornerWidth;
	const float TopLeftBottomY = Margin + ((TopLeftPanel.IsValid() && TopLeftPanel->GetDesiredSize().Y > 10.0f)
		? TopLeftPanel->GetDesiredSize().Y
		: 118.0f);
	if (CardPos.X < TopLeftRightX + 8.0f && CardPos.Y < TopLeftBottomY + 8.0f)
	{
		CardPos.Y = TopLeftBottomY + 8.0f;
	}

	if (LeftControlsCard.IsValid())
	{
		const FVector2D ControlsDesired = LeftControlsCard->GetDesiredSize();
		const float ControlsW = bIsControlsCardExpanded
			? CachedEffectiveControlsWidth
			: ((ControlsDesired.X > 10.0f) ? ControlsDesired.X : 108.0f);
		const float ControlsH = (ControlsDesired.Y > 10.0f)
			? ControlsDesired.Y
			: (bIsControlsCardExpanded ? 218.0f : 28.0f);
		const float ControlsTopY = TopLeftBottomY + 8.0f;
		const float ControlsBottomY = ControlsTopY + ControlsH;
		const float ControlsRightX = Margin + ControlsW;

		if (CardPos.X < ControlsRightX + 8.0f &&
			CardPos.Y + SafeH > ControlsTopY - 6.0f &&
			CardPos.Y < ControlsBottomY + 6.0f)
		{
			CardPos.X = FMath::Min(ControlsRightX + 8.0f, FMath::Max(Margin, LocalSize.X - Margin - SafeW));
		}
	}

	const float TopRightLeftX = LocalSize.X - Margin - CachedEffectiveCornerWidth;
	const float TopRightBottomY = Margin + ((TopRightPanel.IsValid() && TopRightPanel->GetDesiredSize().Y > 10.0f)
		? TopRightPanel->GetDesiredSize().Y
		: 118.0f);

	if (CardPos.X + SafeW > TopRightLeftX - 8.0f && CardPos.Y < TopRightBottomY + 8.0f)
	{
		CardPos.Y = TopRightBottomY + 8.0f;
	}

	const float BottomLeftX = 0.5f * (LocalSize.X - CachedEffectiveBottomWidth);
	const float BottomRightX = BottomLeftX + CachedEffectiveBottomWidth;
	const float BottomTopY = LocalSize.Y - 12.0f - ((BottomCrtConsole.IsValid() && BottomCrtConsole->GetDesiredSize().Y > 10.0f)
		? BottomCrtConsole->GetDesiredSize().Y
		: 108.0f);

	if (CardPos.X < BottomRightX + 8.0f &&
		CardPos.X + SafeW > BottomLeftX - 8.0f &&
		CardPos.Y + SafeH > BottomTopY - 8.0f)
	{
		CardPos.Y = FMath::Max(Margin, BottomTopY - SafeH - 8.0f);
	}

	if (CenterAlertBanner.IsValid() && CenterAlertBanner->GetVisibility() != EVisibility::Collapsed)
	{
		const FVector2D BannerDesired = CenterAlertBanner->GetDesiredSize();
		const float MaxBannerCapW = FMath::Clamp(LocalSize.X * 0.58f, 360.0f, 740.0f);
		const float BannerW = FMath::Clamp((BannerDesired.X > 20.0f) ? BannerDesired.X : 420.0f, 300.0f, MaxBannerCapW);
		const float BannerH = (BannerDesired.Y > 15.0f) ? BannerDesired.Y : 54.0f;
		const float BannerLeftX = 0.5f * (LocalSize.X - BannerW);
		const float BannerRightX = BannerLeftX + BannerW;
		const float BannerTopY = CenterBannerOverlaySlot ? CenterBannerOverlaySlot->GetPadding().Top : (0.595f * LocalSize.Y);
		const float BannerBottomY = BannerTopY + BannerH;

		const bool bOverlapsX = (CardPos.X < BannerRightX + 8.0f) && (CardPos.X + SafeW > BannerLeftX - 8.0f);
		const bool bOverlapsY = (CardPos.Y < BannerBottomY + 6.0f) && (CardPos.Y + SafeH > BannerTopY - 6.0f);
		if (bOverlapsX && bOverlapsY)
		{
			const float ShiftedRightX = BannerRightX + 10.0f;
			if (ShiftedRightX + SafeW <= LocalSize.X - Margin)
			{
				CardPos.X = ShiftedRightX;
			}
			else
			{
				CardPos.Y = FMath::Max(TopRightBottomY + 8.0f, BannerTopY - SafeH - 8.0f);
			}
		}
	}

	ReticleOverlaySlot->SetPadding(FMargin(CardPos.X, CardPos.Y, 0.0f, 0.0f));
}

EActiveTimerReturnType STareTitheHudOverlay::HandleDeferredViewportSync(double /*InCurrentTime*/, float /*InDeltaTime*/)
{
	bViewportSyncScheduled = false;
	if (PendingObservedLocalSize.X > 1.0f && PendingObservedLocalSize.Y > 1.0f)
	{
		SetLocalViewportSize(PendingObservedLocalSize);
	}
	return EActiveTimerReturnType::Stop;
}

void STareTitheHudOverlay::OnArrangeChildren(const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren) const
{
	const FVector2D ObservedLocalSize = AllottedGeometry.GetLocalSize();
	if (ObservedLocalSize.X > 1.0f && ObservedLocalSize.Y > 1.0f)
	{
		CachedUnoverriddenViewportSize = ObservedLocalSize;
	}

	if (!bHasTestViewportSizeOverride &&
		ObservedLocalSize.X > 1.0f &&
		ObservedLocalSize.Y > 1.0f &&
		!ObservedLocalSize.Equals(EffectiveLocalViewportSize, 0.5f) &&
		!bViewportSyncScheduled)
	{
		PendingObservedLocalSize = ObservedLocalSize;
		bViewportSyncScheduled = true;
		STareTitheHudOverlay* MutableSelf = const_cast<STareTitheHudOverlay*>(this);
		MutableSelf->RegisterActiveTimer(
			0.0f,
			FWidgetActiveTimerDelegate::CreateSP(MutableSelf, &STareTitheHudOverlay::HandleDeferredViewportSync));
	}

	SCompoundWidget::OnArrangeChildren(AllottedGeometry, ArrangedChildren);
}

int32 STareTitheHudOverlay::OnPaint(
	const FPaintArgs& Args,
	const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements,
	int32 LayerId,
	const FWidgetStyle& InWidgetStyle,
	bool bParentEnabled) const
{
	const int32 OutLayerId = SCompoundWidget::OnPaint(
		Args,
		AllottedGeometry,
		MyCullingRect,
		OutDrawElements,
		LayerId,
		InWidgetStyle,
		bParentEnabled);

	const FGeometry& ReferenceGeom = (bHasTestViewportSizeOverride && RootOverlay.IsValid())
		? RootOverlay->GetPaintSpaceGeometry()
		: AllottedGeometry;

	// Capture ground-truth painted local coordinates after child CachedGeometry is updated in the identical desktop-space ReferenceGeom.
	LastResolvedLayout.LocalViewportSize = ReferenceGeom.GetLocalSize().X > 1.0f
		? ReferenceGeom.GetLocalSize()
		: EffectiveLocalViewportSize;
	LastResolvedLayout.EffectiveCornerPanelWidth = CachedEffectiveCornerWidth;
	LastResolvedLayout.EffectiveControlsCardWidth = CachedEffectiveControlsWidth;
	LastResolvedLayout.EffectiveBottomConsoleWidth = CachedEffectiveBottomWidth;
	LastResolvedLayout.bControlsCardExpanded = bIsControlsCardExpanded;

	auto RecordChildLocalBounds = [&ReferenceGeom](
		const TSharedPtr<SWidget>& Widget,
		FVector2D& OutMin,
		FVector2D& OutMax) -> bool
	{
		if (!Widget.IsValid() || Widget->GetVisibility() == EVisibility::Collapsed)
		{
			OutMin = FVector2D::ZeroVector;
			OutMax = FVector2D::ZeroVector;
			return false;
		}
		const FGeometry& ChildGeom = Widget->GetPaintSpaceGeometry();
		const FVector2D LocalSize = ChildGeom.GetLocalSize();
		if (LocalSize.X <= 0.0f || LocalSize.Y <= 0.0f)
		{
			return false;
		}
		OutMin = ReferenceGeom.AbsoluteToLocal(ChildGeom.GetAbsolutePosition());
		OutMax = ReferenceGeom.AbsoluteToLocal(ChildGeom.GetAbsolutePositionAtCoordinates(FVector2D(1.0f, 1.0f)));
		return true;
	};

	RecordChildLocalBounds(TopLeftPanel, LastResolvedLayout.TopLeftMin, LastResolvedLayout.TopLeftMax);
	LastResolvedLayout.bControlsCardArranged = RecordChildLocalBounds(
		LeftControlsCard,
		LastResolvedLayout.ControlsCardMin,
		LastResolvedLayout.ControlsCardMax);
	RecordChildLocalBounds(TopRightPanel, LastResolvedLayout.TopRightMin, LastResolvedLayout.TopRightMax);
	RecordChildLocalBounds(BottomCrtConsole, LastResolvedLayout.BottomConsoleMin, LastResolvedLayout.BottomConsoleMax);
	if (BottomCrtConsole.IsValid())
	{
		LastResolvedLayout.BottomConsoleMassBottomLocalY = BottomCrtConsole->GetLastMassReadoutBottomLocalY();
		LastResolvedLayout.BottomConsoleAnalogBarTopLocalY = BottomCrtConsole->GetLastAnalogBarTopLocalY();
	}
	LastResolvedLayout.bCenterBannerArranged = RecordChildLocalBounds(
		CenterAlertBanner,
		LastResolvedLayout.CenterBannerMin,
		LastResolvedLayout.CenterBannerMax);
	LastResolvedLayout.bReticleCardArranged = RecordChildLocalBounds(
		ReticleReadoutCard,
		LastResolvedLayout.ReticleCardMin,
		LastResolvedLayout.ReticleCardMax);

	if (TopLeftPanel.IsValid())
	{
		LastResolvedLayout.TopLeftDesiredWidth = TopLeftPanel->GetDesiredSize().X;
		LastResolvedLayout.TopLeftHeaderBottomLocalY = TopLeftPanel->GetHeaderBottomLocalY();
		LastResolvedLayout.TopLeftDividerLocalY = TopLeftPanel->GetDividerLocalY();
	}
	if (TopRightPanel.IsValid())
	{
		LastResolvedLayout.TopRightDesiredWidth = TopRightPanel->GetDesiredSize().X;
		LastResolvedLayout.TopRightHeaderBottomLocalY = TopRightPanel->GetHeaderBottomLocalY();
		LastResolvedLayout.TopRightDividerLocalY = TopRightPanel->GetDividerLocalY();
	}

	return OutLayerId;
}

void STareTitheHudOverlay::ApplyHudConfig(const FHudPresentationConfig& InConfig)
{
	ActiveConfig = InConfig;
	if (TopLeftPanel.IsValid())
	{
		TopLeftPanel->SetConfig(ActiveConfig);
	}
	if (LeftControlsCard.IsValid())
	{
		LeftControlsCard->SetConfig(ActiveConfig);
	}
	if (TopRightPanel.IsValid())
	{
		TopRightPanel->SetConfig(ActiveConfig);
	}
	if (CenterAlertBanner.IsValid())
	{
		CenterAlertBanner->SetConfig(ActiveConfig);
	}
	if (ReticleReadoutCard.IsValid())
	{
		ReticleReadoutCard->SetConfig(ActiveConfig);
	}
	if (BottomCrtConsole.IsValid())
	{
		BottomCrtConsole->SetConfig(ActiveConfig);
	}
	RefreshResponsiveMetrics();
}

void STareTitheHudOverlay::SetLocalViewportSize(const FVector2D& InLocalViewportSize)
{
	if (bHasTestViewportSizeOverride)
	{
		return;
	}
	const FVector2D ClampedSize(
		FMath::Max(320.0f, InLocalViewportSize.X),
		FMath::Max(240.0f, InLocalViewportSize.Y));
	CachedUnoverriddenViewportSize = ClampedSize;
	if (!EffectiveLocalViewportSize.Equals(ClampedSize, 0.25f))
	{
		EffectiveLocalViewportSize = ClampedSize;
		RefreshResponsiveMetrics();
	}
}

void STareTitheHudOverlay::SetViewportSizeOverrideForTest(const FVector2D& InTestViewportSize, bool bEnableOverride)
{
	bHasTestViewportSizeOverride = bEnableOverride;
	const FVector2D TargetSize = bEnableOverride ? InTestViewportSize : CachedUnoverriddenViewportSize;
	EffectiveLocalViewportSize = FVector2D(
		FMath::Max(320.0f, TargetSize.X),
		FMath::Max(240.0f, TargetSize.Y));
	if (bEnableOverride)
	{
		ChildSlot.HAlign(HAlign_Left).VAlign(VAlign_Top);
		if (ViewportTestOverrideBox.IsValid())
		{
			ViewportTestOverrideBox->SetWidthOverride(EffectiveLocalViewportSize.X);
			ViewportTestOverrideBox->SetHeightOverride(EffectiveLocalViewportSize.Y);
		}
	}
	else
	{
		ChildSlot.HAlign(HAlign_Fill).VAlign(VAlign_Fill);
		if (ViewportTestOverrideBox.IsValid())
		{
			ViewportTestOverrideBox->SetWidthOverride(FOptionalSize());
			ViewportTestOverrideBox->SetHeightOverride(FOptionalSize());
		}
	}
	RefreshResponsiveMetrics();
	Invalidate(EInvalidateWidgetReason::LayoutAndVolatility);
}

void STareTitheHudOverlay::SetControlsCardExpanded(bool bInExpanded)
{
	bIsControlsCardExpanded = bInExpanded;
	if (LeftControlsCard.IsValid())
	{
		LeftControlsCard->SetExpanded(bIsControlsCardExpanded);
	}
	RefreshResponsiveMetrics();
	Invalidate(EInvalidateWidgetReason::LayoutAndVolatility);
}

void STareTitheHudOverlay::SetGraphicsQualityReadout(const FText& InQualityText, bool bInShowRow)
{
	if (LeftControlsCard.IsValid())
	{
		LeftControlsCard->SetGraphicsQualityReadout(InQualityText, bInShowRow);
	}
	RefreshReticleClampedOffset();
}

void STareTitheHudOverlay::UpdateTopLeftPanel(const FTopLeftIndentureViewModel& ViewModel)
{
	if (TopLeftPanel.IsValid())
	{
		TopLeftPanel->ApplyViewModel(ViewModel);
	}
}

void STareTitheHudOverlay::UpdateTopRightPanel(const FTopRightDirectorViewModel& ViewModel)
{
	if (TopRightPanel.IsValid())
	{
		TopRightPanel->ApplyViewModel(ViewModel);
		if (TopRightPanel->GetDesiredSize().Y < 10.0f)
		{
			TopRightPanel->SlatePrepass(1.0f);
		}
		RefreshReticleClampedOffset();
	}
}

void STareTitheHudOverlay::UpdateCenterAlertBanner(const FCenterAlertViewModel& ViewModel)
{
	LastObservedBannerGeneration = ViewModel.TriggerGeneration;
	bIsCenterBannerTerminalStamp = ViewModel.bIsTerminalStamp;
	if (CenterAlertBanner.IsValid())
	{
		CenterAlertBanner->ApplyViewModel(ViewModel);
		if (CenterAlertBanner->GetVisibility() != EVisibility::Collapsed &&
			CenterAlertBanner->GetDesiredSize().X < 20.0f)
		{
			CenterAlertBanner->SlatePrepass(1.0f);
		}
	}
	RefreshCenterBannerOffset();
	RefreshReticleClampedOffset();
}

void STareTitheHudOverlay::UpdateReticleCard(const FReticleReadoutViewModel& ViewModel)
{
	CachedReticleNormalizedUV = ViewModel.NormalizedViewportUV;
	if (ReticleReadoutCard.IsValid())
	{
		ReticleReadoutCard->ApplyViewModel(ViewModel);
		if (ReticleReadoutCard->GetVisibility() != EVisibility::Collapsed && ReticleReadoutCard->GetDesiredSize().X < 20.0f)
		{
			ReticleReadoutCard->SlatePrepass(1.0f);
		}
	}
	RefreshReticleClampedOffset();
}

void STareTitheHudOverlay::UpdateBottomCrtConsole(const FBottomCrtConsoleViewModel& ViewModel)
{
	if (BottomCrtConsole.IsValid())
	{
		BottomCrtConsole->ApplyViewModel(ViewModel);
		if (BottomCrtConsole->GetDesiredSize().Y < 10.0f)
		{
			BottomCrtConsole->SlatePrepass(1.0f);
		}
		RefreshCenterBannerOffset();
		RefreshReticleClampedOffset();
	}
}

bool STareTitheHudOverlay::IsCenterAlertPulseTimerActive() const
{
	return CenterAlertBanner.IsValid() && CenterAlertBanner->IsPulseTimerActive();
}

float STareTitheHudOverlay::GetCenterAlertCurrentBgOpacity() const
{
	return CenterAlertBanner.IsValid() ? CenterAlertBanner->GetCurrentBgOpacity() : 0.0f;
}

float STareTitheHudOverlay::GetCenterAlertCurrentTextOpacity() const
{
	return CenterAlertBanner.IsValid() ? CenterAlertBanner->GetCurrentTextOpacity() : 0.0f;
}
