using UnrealBuildTool;
public class rdcaEditor : ModuleRules
{
 public rdcaEditor(ReadOnlyTargetRules Target) : base(Target)
 {
  PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
  PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine" });
  PrivateDependencyModuleNames.AddRange(new[] { "rdca", "UnrealEd", "AIModule", "AIGraph", "BehaviorTreeEditor", "BlueprintGraph", "KismetCompiler", "AssetRegistry", "GameplayTags" });
 }
}
