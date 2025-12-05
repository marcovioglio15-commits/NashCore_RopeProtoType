// Summary: Game target for RopePrototype runtime build.
using UnrealBuildTool;
using System.Collections.Generic;

public class RopePrototypeTarget : TargetRules
{
    public RopePrototypeTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.V5;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_6;
        CppStandard = CppStandardVersion.Cpp20;
        ExtraModuleNames.Add("RopePrototype");
    }
}
