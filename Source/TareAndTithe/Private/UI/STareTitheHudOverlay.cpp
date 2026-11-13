// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/STareTitheHudOverlay.h"
#include "Layout/ArrangedChildren.h"
#include "Layout/Geometry.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

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
// 1. TOP-LEFT INDENTURE & REQUISITION PANEL
// ============================================================================
class STopLeftIndenturePanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(STopLeftIndenturePanel)
	{
		_Visibility = EVisibility::HitTestInvisible;
	}
	SLATE_END_ARGS()

	void Construct(const FArguments& /*InArgs*/)
	{
		SetVisibility(EVisibility::HitTestInvisible);

		const FSlateFontInfo HeaderFont = FCoreStyle::GetDefaultFontStyle("Bold", 9);
		const FSlateFontInfo DirectiveFont = FCoreStyle::GetDefaultFontStyle("Bold", 10);
		const FSlateFontInfo ScripFont = FCoreStyle::GetDefaultFontStyle("Bold", 9);
		const FSlateFontInfo PillFont = FCoreStyle::GetDefaultFontStyle("Regular", 8);

		ChildSlot
		.Padding(FMargin(12.0f, 9.0f, 10.0f, 9.0f))
		[
			SNew(SVerticalBox)
			.Visibility(EVisibility::HitTestInvisible)

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 4.0f)
			[
				SAssignNew(HeaderTextBlock, STextBlock)
				.Visibility(EVisibility::HitTestInvisible)
				.Font(HeaderFont)
				.ColorAndOpacity(FLinearColor(0.05f, 0.88f, 1.00f, 1.0f))
				.ShadowOffset(FVector2D(1.0f, 1.0f))
				.ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.85f))
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 2.0f, 0.0f, 6.0f)
			[
				SAssignNew(DirectiveTextBlock, STextBlock)
				.Visibility(EVisibility::HitTestInvisible)
				.Font(DirectiveFont)
				.AutoWrapText(true)
				.WrapTextAt(280.0f)
				.ColorAndOpacity(FLinearColor(1.00f, 0.82f, 0.18f, 1.0f))
				.ShadowOffset(FVector2D(1.0f, 1.0f))
				.ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.85f))
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 2.0f, 0.0f, 2.0f)
			[
				SNew(SHorizontalBox)
				.Visibility(EVisibility::HitTestInvisible)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0.0f, 0.0f, 8.0f, 0.0f)
				[
					SAssignNew(ScripTextBlock, STextBlock)
					.Visibility(EVisibility::HitTestInvisible)
					.Font(ScripFont)
					.ColorAndOpacity(FLinearColor(1.00f, 0.82f, 0.18f, 1.0f))
				]

				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.VAlign(VAlign_Center)
				[
					SNew(SHorizontalBox)
					.Visibility(EVisibility::HitTestInvisible)

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(0.0f, 0.0f, 5.0f, 0.0f)
					[
						SAssignNew(UpgradePillBlocks[0], STextBlock)
						.Visibility(EVisibility::HitTestInvisible)
						.Font(PillFont)
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(0.0f, 0.0f, 5.0f, 0.0f)
					[
						SAssignNew(UpgradePillBlocks[1], STextBlock)
						.Visibility(EVisibility::HitTestInvisible)
						.Font(PillFont)
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SAssignNew(UpgradePillBlocks[2], STextBlock)
						.Visibility(EVisibility::HitTestInvisible)
						.Font(PillFont)
					]
				]
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 3.0f, 0.0f, 0.0f)
			[
				SAssignNew(FeedbackTextBlock, STextBlock)
				.Visibility(EVisibility::Collapsed)
				.Font(ScripFont)
				.AutoWrapText(true)
				.WrapTextAt(280.0f)
			]
		];
	}

	void SetConfig(const FHudPresentationConfig& InCfg)
	{
		Config = InCfg;
	}

	void SetWrapWidth(float InWrapWidth)
	{
		const float SafeWrap = FMath::Max(160.0f, InWrapWidth);
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
			FVector2D(3.0f, Size.Y),
			TareTitheHudPaint::WithAlpha(AccentColor, 0.95f));
		TareTitheHudPaint::DrawSolidBox(
			OutDrawElements,
			LayerId + 1,
			AllottedGeometry,
			FVector2D(10.0f, 24.0f),
			FVector2D(FMath::Max(0.0f, Size.X - 20.0f), 1.0f),
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
	float CachedWrapWidth = 280.0f;

	TSharedPtr<STextBlock> HeaderTextBlock;
	TSharedPtr<STextBlock> DirectiveTextBlock;
	TSharedPtr<STextBlock> ScripTextBlock;
	TSharedPtr<STextBlock> UpgradePillBlocks[3];
	TSharedPtr<STextBlock> FeedbackTextBlock;
};

// ============================================================================
// 2. TOP-RIGHT DIRECTOR OVERLOOK PANEL
// ============================================================================
class STopRightDirectorPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(STopRightDirectorPanel)
	{
		_Visibility = EVisibility::HitTestInvisible;
	}
	SLATE_END_ARGS()

	void Construct(const FArguments& /*InArgs*/)
	{
		SetVisibility(EVisibility::HitTestInvisible);

		const FSlateFontInfo HeaderFont = FCoreStyle::GetDefaultFontStyle("Bold", 9);
		const FSlateFontInfo BadgeFont = FCoreStyle::GetDefaultFontStyle("Bold", 8);
		const FSlateFontInfo IntercomFont = FCoreStyle::GetDefaultFontStyle("Bold", 10);

		ChildSlot
		.Padding(FMargin(10.0f, 9.0f, 12.0f, 9.0f))
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
					.ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.85f))
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(6.0f, 0.0f, 0.0f, 0.0f)
				[
					SAssignNew(BadgeTextBlock, STextBlock)
					.Visibility(EVisibility::HitTestInvisible)
					.Font(BadgeFont)
					.ColorAndOpacity(FLinearColor(0.45f, 0.84f, 1.00f, 0.95f))
				]
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 2.0f, 0.0f, 2.0f)
			[
				SAssignNew(IntercomBodyBlock, STextBlock)
				.Visibility(EVisibility::HitTestInvisible)
				.Font(IntercomFont)
				.AutoWrapText(true)
				.WrapTextAt(280.0f)
				.ShadowOffset(FVector2D(1.0f, 1.0f))
				.ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.85f))
			]
		];
	}

	void SetConfig(const FHudPresentationConfig& InCfg)
	{
		Config = InCfg;
	}

	void SetWrapWidth(float InWrapWidth)
	{
		const float SafeWrap = FMath::Max(160.0f, InWrapWidth);
		if (!FMath::IsNearlyEqual(CachedWrapWidth, SafeWrap, 0.5f))
		{
			CachedWrapWidth = SafeWrap;
			if (IntercomBodyBlock.IsValid())
			{
				IntercomBodyBlock->SetWrapTextAt(SafeWrap);
			}
		}
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
			FVector2D(FMath::Max(0.0f, Size.X - 3.0f), 0.0f),
			FVector2D(3.0f, Size.Y),
			TareTitheHudPaint::WithAlpha(AccentColor, 0.95f));
		TareTitheHudPaint::DrawSolidBox(
			OutDrawElements,
			LayerId + 1,
			AllottedGeometry,
			FVector2D(10.0f, 24.0f),
			FVector2D(FMath::Max(0.0f, Size.X - 20.0f), 1.0f),
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
	float CachedWrapWidth = 280.0f;

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
		PrimaryStampFont = FCoreStyle::GetDefaultFontStyle("Bold", 15);
		SecondaryFont = FCoreStyle::GetDefaultFontStyle("Bold", 9);

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
			PrimaryTextBlock->SetFont(VM.bIsTerminalStamp ? PrimaryStampFont : PrimaryHazardFont);
			PrimaryTextBlock->SetText(VM.PrimaryBannerText);
		}

		if (SecondaryTextBlock.IsValid())
		{
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

		// Decision 1: P1 / P2 rising-edge 3-pulse (1.05s) envelope -> steady 0.62f bg / 0.95f text at 0.00 ms idle cost.
		if (VM.TriggerGeneration != LastTriggerGeneration)
		{
			LastTriggerGeneration = VM.TriggerGeneration;
			PulseElapsedSeconds = 0.0f;
			CurrentBgOpacity = 0.90f;
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
		const float TotalEnvelopeSeconds = FMath::Max(0.25f, Config.AlertPulseEnvelopeSeconds);

		if (PulseElapsedSeconds >= TotalEnvelopeSeconds)
		{
			CurrentBgOpacity = Config.AlertPostPulseBackgroundOpacity;
			CurrentTextOpacity = Config.AlertPostPulseTextOpacity;
			CurrentBorderOpacity = 0.85f;
			ApplyCurrentTextColors();
			Invalidate(EInvalidateWidgetReason::Paint);
			ActivePulseTimerHandle.Reset();
			bPulseTimerRunning = false;
			return EActiveTimerReturnType::Stop;
		}

		const float NormT = PulseElapsedSeconds / TotalEnvelopeSeconds;
		const float PulseCount = static_cast<float>(FMath::Max(1, Config.AlertPulseCount));
		const float PulseWave = 0.5f + 0.5f * FMath::Cos(NormT * PulseCount * 2.0f * UE_PI);

		CurrentBgOpacity = FMath::Lerp(0.32f, 0.90f, PulseWave);
		CurrentTextOpacity = FMath::Lerp(0.68f, 1.00f, PulseWave);
		CurrentBorderOpacity = FMath::Lerp(0.42f, 1.00f, PulseWave);
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
	FSlateFontInfo PrimaryStampFont;
	FSlateFontInfo SecondaryFont;
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

		const FSlateFontInfo HeaderFont = FCoreStyle::GetDefaultFontStyle("Bold", 9);
		const FSlateFontInfo WindowFont = FCoreStyle::GetDefaultFontStyle("Regular", 8);
		const FSlateFontInfo MassHeroFont = FCoreStyle::GetDefaultFontStyle("Bold", 17);
		const FSlateFontInfo BadgeFont = FCoreStyle::GetDefaultFontStyle("Bold", 9);
		const FSlateFontInfo StatusFont = FCoreStyle::GetDefaultFontStyle("Bold", 9);
		const FSlateFontInfo LegendFont = FCoreStyle::GetDefaultFontStyle("Regular", 8);

		ChildSlot
		.Padding(FMargin(14.0f, 9.0f, 14.0f, 8.0f))
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
			.Padding(0.0f, 2.0f, 0.0f, 8.0f)
			[
				SNew(SHorizontalBox)
				.Visibility(EVisibility::HitTestInvisible)

				+ SHorizontalBox::Slot()
				.FillWidth(0.30f)
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				[
					SAssignNew(TareBadgeBlock, STextBlock)
					.Visibility(EVisibility::HitTestInvisible)
					.Font(BadgeFont)
				]

				+ SHorizontalBox::Slot()
				.FillWidth(0.40f)
				.HAlign(HAlign_Center)
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
				.FillWidth(0.30f)
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
			.Padding(0.0f, 0.0f, 0.0f, 4.0f)
			[
				SNew(SBox)
				.Visibility(EVisibility::HitTestInvisible)
				.HeightOverride(7.0f)
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Center)
			.Padding(0.0f, 1.0f, 0.0f, 4.0f)
			[
				SAssignNew(OperationalStatusBlock, STextBlock)
				.Visibility(EVisibility::HitTestInvisible)
				.Justification(ETextJustify::Center)
				.Font(StatusFont)
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Center)
			[
				SAssignNew(ControlLegendBlock, STextBlock)
				.Visibility(EVisibility::HitTestInvisible)
				.Justification(ETextJustify::Center)
				.Font(LegendFont)
				.ColorAndOpacity(FLinearColor(0.84f, 0.82f, 0.72f, 0.88f))
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
		if (ControlLegendBlock.IsValid())
		{
			ControlLegendBlock->SetText(VM.ControlLegendText);
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

		const float BarLeftX = 16.0f;
		const float BarWidth = FMath::Max(0.0f, Size.X - 32.0f);
		const float BarTopY = FMath::Max(40.0f, Size.Y - 47.0f);
		const float BarHeight = 5.0f;
		if (BarWidth > 20.0f)
		{
			TareTitheHudPaint::DrawSolidBox(
				OutDrawElements,
				LayerId + 3,
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
					LayerId + 3,
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
					LayerId + 3,
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
					LayerId + 4,
					AllottedGeometry,
					FVector2D(BarLeftX, BarTopY),
					FVector2D(FillW, BarHeight),
					TareTitheHudPaint::WithAlpha(CurrentVM.MassReadoutColor, 0.92f));
			}
		}

		return SCompoundWidget::OnPaint(
			Args,
			AllottedGeometry,
			MyCullingRect,
			OutDrawElements,
			LayerId + 5,
			InWidgetStyle,
			bParentEnabled);
	}

private:
	FHudPresentationConfig Config;
	FBottomCrtConsoleViewModel CurrentVM;

	TSharedPtr<STextBlock> ConsoleHeaderBlock;
	TSharedPtr<STextBlock> WindowsSummaryBlock;
	TSharedPtr<STextBlock> MassReadoutBlock;
	TSharedPtr<STextBlock> TareBadgeBlock;
	TSharedPtr<STextBlock> RouteBadgeBlock;
	TSharedPtr<STextBlock> OperationalStatusBlock;
	TSharedPtr<STextBlock> ControlLegendBlock;
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
	SetVisibility(EVisibility::HitTestInvisible);

	SAssignNew(TopLeftPanel, STopLeftIndenturePanel);
	SAssignNew(TopRightPanel, STopRightDirectorPanel);
	SAssignNew(CenterAlertBanner, SCenterAlertBanner);
	SAssignNew(ReticleReadoutCard, SReticleReadoutCard);
	SAssignNew(BottomCrtConsole, SBottomCrtWeighingConsole);

	ChildSlot
	[
		SAssignNew(RootOverlay, SOverlay)
		.Visibility(EVisibility::HitTestInvisible)

		+ SOverlay::Slot()
		.Expose(TopLeftOverlaySlot)
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Top)
		[
			SAssignNew(TopLeftWidthBox, SBox)
			.Visibility(EVisibility::HitTestInvisible)
			[
				TopLeftPanel.ToSharedRef()
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
	];

	ApplyHudConfig(ActiveConfig);
}

void STareTitheHudOverlay::RefreshResponsiveMetrics()
{
	const float LocalW = FMath::Max(320.0f, EffectiveLocalViewportSize.X);
	const float Margin = FMath::Clamp(ActiveConfig.CornerPanelMarginPx, 6.0f, 40.0f);
	const float MaxCornerFrac = FMath::Clamp(ActiveConfig.CornerPanelMaxViewportWidthFraction, 0.14f, 0.28f);

	// Correction B: FMath::Clamp(FMath::Min(HudConfig.CornerPanelWidthPx, LocalViewportSize.X * 0.20f), 250.0f, HudConfig.CornerPanelWidthPx)
	// with safety ceiling if docked pane is narrower than 1250 local units.
	const float SafeMinFloor = FMath::Min(250.0f, LocalW * MaxCornerFrac);
	CachedEffectiveCornerWidth = FMath::Clamp(
		FMath::Min(ActiveConfig.CornerPanelWidthPx, LocalW * MaxCornerFrac),
		SafeMinFloor,
		ActiveConfig.CornerPanelWidthPx);

	const float WrapW = FMath::Max(160.0f, CachedEffectiveCornerWidth - 24.0f);
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
		TopLeftPanel->SetWrapWidth(WrapW);
	}
	if (TopRightPanel.IsValid())
	{
		TopRightPanel->SetWrapWidth(WrapW);
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
		CenterBannerWidthBox->SetMaxDesiredWidth(FMath::Clamp(LocalW * 0.50f, 320.0f, 640.0f));
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

	const float TopRightLeftX = LocalSize.X - Margin - CachedEffectiveCornerWidth;
	const float TopRightBottomY = Margin + ((TopRightPanel.IsValid() && TopRightPanel->GetDesiredSize().Y > 10.0f)
		? TopRightPanel->GetDesiredSize().Y
		: 86.0f);

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
		const float BannerW = FMath::Clamp((BannerDesired.X > 20.0f) ? BannerDesired.X : 340.0f, 300.0f, LocalSize.X * 0.50f);
		const float BannerH = (BannerDesired.Y > 15.0f) ? BannerDesired.Y : 50.0f;
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
	if (ObservedLocalSize.X > 1.0f &&
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

	// Capture ground-truth painted local coordinates after child CachedGeometry is updated in the identical desktop-space AllottedGeometry.
	LastResolvedLayout.LocalViewportSize = AllottedGeometry.GetLocalSize();
	LastResolvedLayout.EffectiveCornerPanelWidth = CachedEffectiveCornerWidth;
	LastResolvedLayout.EffectiveBottomConsoleWidth = CachedEffectiveBottomWidth;

	auto RecordChildLocalBounds = [&AllottedGeometry](
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
		OutMin = AllottedGeometry.AbsoluteToLocal(ChildGeom.GetAbsolutePosition());
		OutMax = AllottedGeometry.AbsoluteToLocal(ChildGeom.GetAbsolutePositionAtCoordinates(FVector2D(1.0f, 1.0f)));
		return true;
	};

	RecordChildLocalBounds(TopLeftPanel, LastResolvedLayout.TopLeftMin, LastResolvedLayout.TopLeftMax);
	RecordChildLocalBounds(TopRightPanel, LastResolvedLayout.TopRightMin, LastResolvedLayout.TopRightMax);
	RecordChildLocalBounds(BottomCrtConsole, LastResolvedLayout.BottomConsoleMin, LastResolvedLayout.BottomConsoleMax);
	LastResolvedLayout.bCenterBannerArranged = RecordChildLocalBounds(
		CenterAlertBanner,
		LastResolvedLayout.CenterBannerMin,
		LastResolvedLayout.CenterBannerMax);
	LastResolvedLayout.bReticleCardArranged = RecordChildLocalBounds(
		ReticleReadoutCard,
		LastResolvedLayout.ReticleCardMin,
		LastResolvedLayout.ReticleCardMax);

	return OutLayerId;
}

void STareTitheHudOverlay::ApplyHudConfig(const FHudPresentationConfig& InConfig)
{
	ActiveConfig = InConfig;
	if (TopLeftPanel.IsValid())
	{
		TopLeftPanel->SetConfig(ActiveConfig);
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
	const FVector2D ClampedSize(
		FMath::Max(320.0f, InLocalViewportSize.X),
		FMath::Max(240.0f, InLocalViewportSize.Y));
	if (!EffectiveLocalViewportSize.Equals(ClampedSize, 0.25f))
	{
		EffectiveLocalViewportSize = ClampedSize;
		RefreshResponsiveMetrics();
	}
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
		RefreshReticleClampedOffset();
	}
}

void STareTitheHudOverlay::UpdateCenterAlertBanner(const FCenterAlertViewModel& ViewModel)
{
	bIsCenterBannerTerminalStamp = ViewModel.bIsTerminalStamp;
	if (CenterAlertBanner.IsValid())
	{
		CenterAlertBanner->ApplyViewModel(ViewModel);
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
	}
	RefreshReticleClampedOffset();
}

void STareTitheHudOverlay::UpdateBottomCrtConsole(const FBottomCrtConsoleViewModel& ViewModel)
{
	if (BottomCrtConsole.IsValid())
	{
		BottomCrtConsole->ApplyViewModel(ViewModel);
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
