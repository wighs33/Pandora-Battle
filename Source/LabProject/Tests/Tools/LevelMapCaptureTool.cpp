// 레벨 지도 찍기 도구: 맵 목록(DA_Level)의 MapLayout대로 레벨을 정사영으로 찍어 지도 그림의 재료를 남긴다.
// 테스트 묶음(Scripts/RunAutomationTests.ps1)에는 들지 않는 도구다. 그림 결과는 Scripts/MapDesign/level_map_style.py가 칠한다.
//
// 실행(1080p 오프스크린, 땅속 지역을 자르는 클립 평면 허용):
//   UnrealEditor.exe <uproject> -game -RenderOffscreen -windowed -ResX=1920 -ResY=1080 -ForceRes -unattended -nosteam -nosound
//     "-ini:Engine:[/Script/Engine.RendererSettings]:r.AllowGlobalClipPlane=True"
//     -ExecCmds="wp.Runtime.OverrideRuntimeSpatialHashLoadingRange 1000000,Automation RunTests LabProject.Tools.MapCapture.<맵 키>;Quit"
// 결과: Saved/MapStyle/raw/<맵 키>/
//   page<n>_{base,normal,depth}.f32  지역 지도마다 바닥색·월드 법선·깊이(머리 "W H 4\n" + RGBA float32)
//   area_{base,normal,depth}.f32     지역이 여럿이면 전체 지도
//   thumbnail.png                    맵 썸네일(UI 없이 같은 카메라 규칙으로)
//   manifest.json                    찍은 카메라와 지역 트리거 상자(지역 빛을 칠할 때 쓴다)
#include "Tests/Integration/IntegrationTestHelpers.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITORONLY_DATA

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/StaticMeshComponent.h"
#include "Definition/Level/LevelDefinition.h"
#include "Dom/JsonObject.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "EngineUtils.h"
#include "HAL/FileManager.h"
#include "Kismet/GameplayStatics.h"
#include "Map/PlayerMapRegionTrigger.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UnrealClient.h"

namespace LevelMapCapture
{
	/** 지도 그림의 긴 변(px). 칠하기 필터의 선 문턱이 이 크기에 맞춰져 있다. */
	constexpr int32 LongSide = 1024;
	/** 정사영 카메라를 보는 점에서 이만큼 뒤로 뺀다(cm). 깊이에서 높이를 되살릴 때 manifest에 같이 적는다. */
	constexpr float CameraDistance = 60000.0f;
	/** 썸네일 카메라: 놀이 공간 가운데를 이 각도로 내려다보고, 놀이 공간 반지름이 화각에 이만큼 여유 있게 든다. */
	constexpr float ThumbnailPitch = -35.0f;
	constexpr float ThumbnailFieldOfView = 50.0f;
	constexpr float ThumbnailMargin = 1.1f;
	/** 레벨이 열린 뒤 스트리밍·그림자가 자리 잡을 때까지 기다린다(초). */
	constexpr float SettleSeconds = 8.0f;

	FString OutDir(const FString& MapKey)
	{
		return FPaths::ProjectSavedDir() / TEXT("MapStyle/raw") / MapKey;
	}

	void WriteFloatImage(const FString& Path, const TArray<FLinearColor>& Pixels, const FIntPoint Size)
	{
		TArray<uint8> Bytes;
		const FTCHARToUTF8 Header(*FString::Printf(TEXT("%d %d 4\n"), Size.X, Size.Y));
		Bytes.Append(reinterpret_cast<const uint8*>(Header.Get()), Header.Length());
		Bytes.Append(reinterpret_cast<const uint8*>(Pixels.GetData()), Pixels.Num() * sizeof(FLinearColor));
		FFileHelper::SaveArrayToFile(Bytes, *Path);
	}

	struct FOrthoShot
	{
		FVector LookAt = FVector::ZeroVector;
		FRotator Rotation = FRotator::ZeroRotator;
		float WorldWidth = 10000.0f;
		FIntPoint Size = FIntPoint(LongSide, LongSide);
		TOptional<float> ClipAbove;
	};

	/** 정사영으로 바닥색·법선·깊이를 찍어 Prefix_*.f32로 남긴다. */
	void Capture(UWorld& World, const FOrthoShot& Shot, const FString& Prefix)
	{
		UTextureRenderTarget2D* Target = NewObject<UTextureRenderTarget2D>();
		Target->RenderTargetFormat = RTF_RGBA32f;
		Target->InitAutoFormat(Shot.Size.X, Shot.Size.Y);
		Target->UpdateResourceImmediate(true);

		ASceneCapture2D* CaptureActor = World.SpawnActor<ASceneCapture2D>();
		USceneCaptureComponent2D* Component = CaptureActor->GetCaptureComponent2D();
		Component->TextureTarget = Target;
		Component->bCaptureEveryFrame = false;
		Component->bCaptureOnMovement = false;
		Component->ProjectionType = ECameraProjectionMode::Orthographic;
		Component->OrthoWidth = Shot.WorldWidth;
		Component->ShowFlags.SetFog(false);
		Component->ShowFlags.SetVolumetricFog(false);
		Component->ShowFlags.SetAtmosphere(false);
		Component->ShowFlags.SetCloud(false);
		Component->ShowFlags.SetParticles(false);
		if (Shot.ClipAbove.IsSet())
		{
			Component->bEnableClipPlane = true;
			Component->ClipPlaneBase = FVector(Shot.LookAt.X, Shot.LookAt.Y, Shot.ClipAbove.GetValue());
			Component->ClipPlaneNormal = FVector(0.0f, 0.0f, -1.0f);
		}
		CaptureActor->SetActorLocationAndRotation(Shot.LookAt - Shot.Rotation.Vector() * CameraDistance, Shot.Rotation);

		const TPair<ESceneCaptureSource, const TCHAR*> Sources[] = {
			{SCS_BaseColor, TEXT("base")}, {SCS_Normal, TEXT("normal")}, {SCS_SceneDepth, TEXT("depth")}};
		for (const TPair<ESceneCaptureSource, const TCHAR*>& Source : Sources)
		{
			Component->CaptureSource = Source.Key;
			Component->CaptureScene();
			FlushRenderingCommands();
			TArray<FLinearColor> Pixels;
			Target->GameThread_GetRenderTargetResource()->ReadLinearColorPixels(Pixels, FReadSurfaceDataFlags(RCM_MinMax));
			WriteFloatImage(FString::Printf(TEXT("%s_%s.f32"), *Prefix, Source.Value), Pixels, Shot.Size);
		}
		CaptureActor->Destroy();
	}

	/** 지역 지도: 똑바로 내려다보고, 그림 오른쪽이 페이지 방향각이 되게 카메라 방향각을 90도 뺀다. */
	FOrthoShot PageShot(const FLevelMapPage& Page)
	{
		FOrthoShot Shot;
		Shot.LookAt = FVector(Page.WorldCenter, 0.0f);
		Shot.Rotation = FRotator(-90.0f, Page.WorldYaw - 90.0f, 0.0f);
		Shot.WorldWidth = Page.WorldSize.X;
		const float Aspect = Page.WorldSize.X / FMath::Max(Page.WorldSize.Y, 1.0f);
		Shot.Size = Aspect >= 1.0f ? FIntPoint(LongSide, FMath::RoundToInt(LongSide / Aspect)) : FIntPoint(FMath::RoundToInt(LongSide * Aspect), LongSide);
		if (Page.bCaptureClipAbove)
		{
			Shot.ClipAbove = Page.CaptureClipHeight;
		}
		return Shot;
	}

	FOrthoShot AreaShot(const FLevelMapAreaView& Area)
	{
		FOrthoShot Shot;
		Shot.LookAt = Area.Center;
		Shot.Rotation = FRotator(Area.Pitch, Area.Yaw, 0.0f);
		Shot.WorldWidth = Area.WorldWidth;
		return Shot;
	}

	TSharedRef<FJsonValue> VectorJson(const FVector& Value)
	{
		return MakeShared<FJsonValueArray>(TArray<TSharedPtr<FJsonValue>>{
			MakeShared<FJsonValueNumber>(Value.X), MakeShared<FJsonValueNumber>(Value.Y), MakeShared<FJsonValueNumber>(Value.Z)});
	}

	/** 칠하기 스크립트가 쓰는 찍은 카메라와 지역 트리거 상자. */
	void WriteManifest(UWorld& World, const FString& MapKey, const FLevelMapLayout& Layout)
	{
		const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
		Root->SetStringField(TEXT("map_key"), MapKey);
		Root->SetNumberField(TEXT("camera_distance"), CameraDistance);
		TArray<TSharedPtr<FJsonValue>> Pages;
		for (const FLevelMapPage& Page : Layout.Pages)
		{
			const FOrthoShot Shot = PageShot(Page);
			const TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
			Entry->SetNumberField(TEXT("region"), static_cast<int32>(Page.Region));
			Entry->SetField(TEXT("look_at"), VectorJson(Shot.LookAt));
			Entry->SetNumberField(TEXT("yaw"), Shot.Rotation.Yaw);
			Entry->SetNumberField(TEXT("pitch"), Shot.Rotation.Pitch);
			Entry->SetNumberField(TEXT("world_width"), Shot.WorldWidth);
			Pages.Add(MakeShared<FJsonValueObject>(Entry));
		}
		Root->SetArrayField(TEXT("pages"), Pages);
		if (Layout.Pages.Num() > 1)
		{
			const FOrthoShot Shot = AreaShot(Layout.AreaView);
			const TSharedRef<FJsonObject> Area = MakeShared<FJsonObject>();
			Area->SetField(TEXT("look_at"), VectorJson(Shot.LookAt));
			Area->SetNumberField(TEXT("yaw"), Shot.Rotation.Yaw);
			Area->SetNumberField(TEXT("pitch"), Shot.Rotation.Pitch);
			Area->SetNumberField(TEXT("world_width"), Shot.WorldWidth);
			Root->SetObjectField(TEXT("area"), Area);
		}
		TArray<TSharedPtr<FJsonValue>> Triggers;
		for (TActorIterator<APlayerMapRegionTrigger> It(&World); It; ++It)
		{
			const UBoxComponent* Box = It->FindComponentByClass<UBoxComponent>();
			const FProperty* RegionProperty = It->GetClass()->FindPropertyByName(TEXT("TargetMapRegion"));
			if (!Box || !RegionProperty)
			{
				continue;
			}
			const TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
			Entry->SetNumberField(TEXT("region"), *RegionProperty->ContainerPtrToValuePtr<uint8>(*It));
			Entry->SetField(TEXT("location"), VectorJson(Box->GetComponentLocation()));
			const FRotator Rotation = Box->GetComponentRotation();
			Entry->SetField(TEXT("rotation"), VectorJson(FVector(Rotation.Pitch, Rotation.Yaw, Rotation.Roll)));
			Entry->SetField(TEXT("extent"), VectorJson(Box->GetScaledBoxExtent()));
			Triggers.Add(MakeShared<FJsonValueObject>(Entry));
		}
		Root->SetArrayField(TEXT("triggers"), Triggers);
		FString Text;
		FJsonSerializer::Serialize(Root, TJsonWriterFactory<>::Create(&Text));
		FFileHelper::SaveStringToFile(Text, *(OutDir(MapKey) / TEXT("manifest.json")));
	}

	/** 지역이 여럿인 레벨에서 땅이 전체 지도 그림에서 차지하는 비율(썸네일은 그 땅만 담는다). */
	constexpr float AreaLandFraction = 0.6f;

	/**
	 * 썸네일이 바라볼 놀이 공간. 지역이 여럿이면 전체 지도 카메라가 보는 곳(지역 지도마다 넓이가 달라 그 합은 한쪽으로 쏠린다),
	 * 한 장이면 그 지도의 월드 사각형. 높이는 그 안 정적 메시의 가운데.
	 */
	FBox PlaySpace(UWorld& World, const FLevelMapLayout& Layout)
	{
		FBox Box(ForceInit);
		if (Layout.Pages.Num() > 1)
		{
			const float Radius = Layout.AreaView.WorldWidth * 0.5f * AreaLandFraction;
			Box = FBox(Layout.AreaView.Center - FVector(Radius, Radius, 0.0f), Layout.AreaView.Center + FVector(Radius, Radius, 0.0f));
		}
		for (const FLevelMapPage& Page : Layout.Pages.Num() == 1 ? Layout.Pages : TArray<FLevelMapPage>())
		{
			const float Radius = Page.WorldSize.GetMax() * 0.5f;
			Box += FBox(FVector(Page.WorldCenter - FVector2D(Radius), 0.0f), FVector(Page.WorldCenter + FVector2D(Radius), 0.0f));
		}
		TArray<float> Heights;
		for (TActorIterator<AActor> It(&World); It; ++It)
		{
			const FVector Location = It->GetActorLocation();
			if (It->FindComponentByClass<UStaticMeshComponent>() && Box.IsInsideXY(Location))
			{
				Heights.Add(Location.Z);
			}
		}
		Heights.Sort();
		const float Height = Heights.IsEmpty() ? Box.GetCenter().Z : Heights[Heights.Num() / 2];
		Box.Min.Z = Box.Max.Z = Height;
		return Box;
	}

	void AimThumbnailCamera(UWorld& World, const FLevelMapLayout& Layout)
	{
		const FBox Space = PlaySpace(World, Layout);
		const FRotator Rotation(ThumbnailPitch, Layout.ThumbnailYaw, 0.0f);
		const float Radius = FMath::Max(Space.GetExtent().X, Space.GetExtent().Y);
		const float Distance = Radius / FMath::Tan(FMath::DegreesToRadians(ThumbnailFieldOfView * 0.5f)) * ThumbnailMargin;
		ACameraActor* Camera = World.SpawnActor<ACameraActor>(Space.GetCenter() - Rotation.Vector() * Distance, Rotation);
		Camera->GetCameraComponent()->SetFieldOfView(ThumbnailFieldOfView);
		if (APlayerController* Controller = World.GetFirstPlayerController())
		{
			if (APawn* Pawn = Controller->GetPawn())
			{
				Pawn->SetActorHiddenInGame(true);
			}
			Controller->SetViewTarget(Camera);
		}
	}
}

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FPdLevelMapCaptureTool, "LabProject.Tools.MapCapture",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

void FPdLevelMapCaptureTool::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
{
	const ULevelDefinition* Levels = Cast<ULevelDefinition>(ULevelDefinition::GetDefaultDefinitionPath().TryLoad());
	if (!Levels)
	{
		return;
	}
	for (const FLobbyMatchMapOption& Option : Levels->IngameLevels)
	{
		if (Option.MapLayout.HasMap())
		{
			OutBeautifiedNames.Add(Option.MapKey.ToString());
			OutTestCommands.Add(Option.MapKey.ToString());
		}
	}
}

bool FPdLevelMapCaptureTool::RunTest(const FString& Parameters)
{
	using namespace LevelMapCapture;
	const ULevelDefinition* Levels = Cast<ULevelDefinition>(ULevelDefinition::GetDefaultDefinitionPath().TryLoad());
	FLobbyMatchMapOption Option;
	if (!Levels || !Levels->FindIngameLevel(FName(*Parameters), Option) || !Option.MapLayout.HasMap())
	{
		AddError(FString::Printf(TEXT("%s has no map layout in the level definition"), *Parameters));
		return false;
	}
	const FString MapKey = Option.MapKey.ToString();
	const FString MapPath = Option.Map.ToSoftObjectPath().GetLongPackageName();
	IFileManager::Get().MakeDirectory(*OutDir(MapKey), true);

	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([MapPath]()
	{
		UGameplayStatics::OpenLevel(IntegrationTest::GetGameWorld(), FName(*MapPath));
		return true;
	}));
	IntegrationTest::WaitUntil(this, MapPath, 60.0f, [MapPath]()
	{
		const UWorld* World = IntegrationTest::GetGameWorld();
		return World && World->HasBegunPlay() && World->GetOutermost()->GetName() == MapPath;
	});
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(SettleSeconds));
	IntegrationTest::Step(0.5f, [MapKey, Layout = Option.MapLayout]()
	{
		UWorld* World = IntegrationTest::GetGameWorld();
		for (int32 PageIndex = 0; PageIndex < Layout.Pages.Num(); ++PageIndex)
		{
			Capture(*World, PageShot(Layout.Pages[PageIndex]), OutDir(MapKey) / FString::Printf(TEXT("page%d"), PageIndex));
		}
		if (Layout.Pages.Num() > 1)
		{
			Capture(*World, AreaShot(Layout.AreaView), OutDir(MapKey) / TEXT("area"));
		}
		WriteManifest(*World, MapKey, Layout);
		AimThumbnailCamera(*World, Layout);
	});
	IntegrationTest::Step(2.0f, [MapKey]()
	{
		FScreenshotRequest::RequestScreenshot(OutDir(MapKey) / TEXT("thumbnail.png"), false, false);
	});
	IntegrationTest::Step(1.0f, []() {});
	return true;
}

#endif
