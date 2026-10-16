#include "RDCABossAIAssetsCommandlet.h"
#include "Boss/AI/RDCABossAIController.h"
#include "Boss/RDCABossPawn.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardData.h"
#include "BehaviorTreeGraph.h"
#include "BehaviorTreeGraphNode_Root.h"
#include "EdGraphSchema_BehaviorTree.h"
#include "EdGraph/EdGraphPin.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
bool Save(UObject* Asset)
{
 Asset->SetFlags(RF_Public | RF_Standalone); Asset->MarkPackageDirty();
 FSavePackageArgs Args; Args.TopLevelFlags = RF_Public | RF_Standalone; Args.SaveFlags = SAVE_NoError;
 const FString File = FPackageName::LongPackageNameToFilename(Asset->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension());
 return UPackage::SavePackage(Asset->GetOutermost(), Asset, *File, Args);
}
void Layout(UEdGraphNode* Node, int32 Depth, int32& X)
{
 int32 First = X; bool HasChild = false;
 for (UEdGraphPin* Pin : Node->Pins)
 {
  if (Pin->Direction != EGPD_Output) { continue; }
  for (UEdGraphPin* Next : Pin->LinkedTo) { Layout(Next->GetOwningNode(), Depth + 1, X); HasChild = true; }
 }
 if (!HasChild) { X += 430; }
 Node->NodePosX = (First + X - 430) / 2; Node->NodePosY = Depth * 360;
}
}
URDCABossAIAssetsCommandlet::URDCABossAIAssetsCommandlet()
{
 IsClient = false; IsServer = false; IsEditor = true; LogToConsole = true;
}
int32 URDCABossAIAssetsCommandlet::Main(const FString& Params)
{
 const FString TreePath = TEXT("/Game/AI/BT_RiftDragonCombat");
 const FString BBPath = TEXT("/Game/AI/BB_RiftDragonCombat");
 const FString ControllerPath = TEXT("/Game/AI/BP_RiftDragonAIController");
 // Never replace an existing designer-authored graph. Verification is read-only.
 if (FPackageName::DoesPackageExist(TreePath) || FPackageName::DoesPackageExist(BBPath) || FPackageName::DoesPackageExist(ControllerPath))
 {
  if (!FParse::Param(*Params, TEXT("VerifyOnly")) && !FParse::Param(*Params, TEXT("ConfigureBoss")))
  { UE_LOG(LogTemp, Error, TEXT("AI assets already exist; refusing to overwrite designer assets. Use -VerifyOnly.")); return 2; }
  auto* Tree = LoadObject<UBehaviorTree>(nullptr, *(TreePath + TEXT(".BT_RiftDragonCombat")));
  auto* BB = LoadObject<UBlackboardData>(nullptr, *(BBPath + TEXT(".BB_RiftDragonCombat")));
  auto* ControllerClass = LoadClass<ARDCABossAIController>(nullptr, *(ControllerPath + TEXT(".BP_RiftDragonAIController_C")));
  if (!Tree || !Tree->RootNode || !Tree->BTGraph || Tree->BlackboardAsset != BB || !ControllerClass) { return 5; }
  auto* Property = FindFProperty<FObjectPropertyBase>(ControllerClass, TEXT("CombatBehaviorTree"));
  if (!Property || Property->GetObjectPropertyValue_InContainer(ControllerClass->GetDefaultObject()) != Tree) { return 6; }
  auto* BossBP = LoadObject<UBlueprint>(nullptr, TEXT("/Game/Blueprints/Boss/BP_BossPlaceholder.BP_BossPlaceholder"));
  auto* Boss = BossBP && BossBP->GeneratedClass ? Cast<ARDCABossPawn>(BossBP->GeneratedClass->GetDefaultObject()) : nullptr;
  if (!Boss) { UE_LOG(LogTemp, Error, TEXT("Existing Boss Blueprint is not a RDCABossPawn.")); return 7; }
  UE_LOG(LogTemp, Display, TEXT("Verified graph=%d keys=%d; Boss Controller=%s AutoAI=%d AutoPlayer=%d"),
   Tree->BTGraph->Nodes.Num(), BB->Keys.Num(), *GetNameSafe(Boss->AIControllerClass.Get()), static_cast<int32>(Boss->AutoPossessAI), static_cast<int32>(Boss->AutoPossessPlayer));
  if (FParse::Param(*Params, TEXT("ConfigureBoss")))
  {
   const FString Source = FPackageName::LongPackageNameToFilename(BossBP->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension());
   const FString Backup = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Backups/StandardBossAI"),
    FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")), TEXT("BP_BossPlaceholder.uasset"));
   IFileManager::Get().MakeDirectory(*FPaths::GetPath(Backup), true);
   if (IFileManager::Get().Copy(*Backup, *Source, false) != COPY_OK) { return 8; }
   Boss->AIControllerClass = ControllerClass;
   Boss->AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
   Boss->AutoPossessPlayer = EAutoReceiveInput::Disabled;
   if (!Save(BossBP)) { return 9; }
   UE_LOG(LogTemp, Display, TEXT("Configured existing Boss AI defaults only. Preserved mesh/components/config. Backup=%s"), *Backup);
  }
  return 0;
 }
 auto* Tree = ARDCABossAIController::CreateDefaultCombatTree(CreatePackage(*TreePath));
 TStrongObjectPtr<UBehaviorTree> KeepTree(Tree);
 auto* BB = Tree->BlackboardAsset.Get();
 TStrongObjectPtr<UBlackboardData> KeepBlackboard(BB);
 BB->Rename(TEXT("BB_RiftDragonCombat"), CreatePackage(*BBPath), REN_DontCreateRedirectors | REN_NonTransactional);
 Tree->BTGraph = FBlueprintEditorUtils::CreateNewGraph(Tree, TEXT("BehaviorTreeGraph"), UBehaviorTreeGraph::StaticClass(), UEdGraphSchema_BehaviorTree::StaticClass());
 auto* Graph = CastChecked<UBehaviorTreeGraph>(Tree->BTGraph);
 Graph->GetSchema()->CreateDefaultNodesForGraph(*Graph);
 Graph->OnCreated(); Graph->Initialize(); Graph->MarkVersion();
 int32 X = 0;
 for (UEdGraphNode* Node : Graph->Nodes) { if (Node->IsA<UBehaviorTreeGraphNode_Root>()) { Layout(Node, 0, X); break; } }
 Graph->OnSave();
 auto* BP = FKismetEditorUtilities::CreateBlueprint(ARDCABossAIController::StaticClass(), CreatePackage(*ControllerPath), TEXT("BP_RiftDragonAIController"), BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass());
 TStrongObjectPtr<UBlueprint> KeepBlueprint(BP);
 FKismetEditorUtilities::CompileBlueprint(BP);
 if (!BP->GeneratedClass) { return 3; }
 auto* Property = FindFProperty<FObjectPropertyBase>(ARDCABossAIController::StaticClass(), TEXT("CombatBehaviorTree"));
 Property->SetObjectPropertyValue_InContainer(BP->GeneratedClass->GetDefaultObject(), Tree);
 for (UObject* Asset : { static_cast<UObject*>(BB), static_cast<UObject*>(Tree), static_cast<UObject*>(BP) })
 {
  FAssetRegistryModule::AssetCreated(Asset);
  if (!Save(Asset)) { UE_LOG(LogTemp, Error, TEXT("Failed to save %s"), *Asset->GetPathName()); return 4; }
 }
 UE_LOG(LogTemp, Display, TEXT("Created visual Boss BT/BB/controller. GraphNodes=%d BlackboardKeys=%d"), Graph->Nodes.Num(), BB->Keys.Num());
 return 0;
}
