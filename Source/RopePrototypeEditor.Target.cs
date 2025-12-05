// Summary: Editor target for RopePrototype.
using UnrealBuildTool;
using System.Collections.Generic;

public class RopePrototypeEditorTarget : TargetRules
{
    public RopePrototypeEditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.V5;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_6;
        CppStandard = CppStandardVersion.Cpp20;
        ExtraModuleNames.Add("RopePrototype");
    }
}
