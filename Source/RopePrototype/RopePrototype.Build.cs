// Summary: Build rules for the RopePrototype module.
using UnrealBuildTool;

public class RopePrototype : ModuleRules
{
    public RopePrototype(ReadOnlyTargetRules Target) : base(Target)
    {
        // Engine core dependencies kept minimal for runtime module.
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "InputCore"
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "Slate",
            "SlateCore"
        });
    }
}
