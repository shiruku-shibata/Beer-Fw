Import("env")
import os

lib_path = os.path.join(
    env.subst("$PROJECT_LIBDEPS_DIR"),
    env.subst("$PIOENV"),
    "DFRobot_GP8XXX",
    "DFRobot_GP8XXX.cpp"
)

if os.path.exists(lib_path):
    with open(lib_path, "r") as f:
        content = f.read()
    
    patched = content.replace(
        "analogWriteResolution(_pin0,10);",
        "analogWriteResolution(10);"
    )
    
    if patched != content:
        with open(lib_path, "w") as f:
            f.write(patched)
        print("✅ DFRobot_GP8XXX patched successfully")
    else:
        print("ℹ️ DFRobot_GP8XXX already patched or pattern not found")