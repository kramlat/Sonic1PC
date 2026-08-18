cmake_minimum_required(VERSION 3.16) # Qt6's CMake package/AUTOMOC requires >= 3.16

# ##############################################################################
# Options #
# ##############################################################################
set(BACKEND
    "SDL2"
    CACHE STRING "Which backend to use (SDL2)")
option(REV01 "Compile REV01 ROM" ON)
option(JAPANESE "Compile Japanese ROM" OFF)
option(FIX_BUGS "Fix bugs (completely screwed up code, not gameplay bugs)" OFF)

# Two custom CMAKE_BUILD_TYPEs (alongside the usual Debug/Release/etc), both
# defaulting SPLASH (below) to ON -- boot straight into the SSRG (Sonic
# Stuff Research Group) screen
# instead of the title screen, for a polished "preview build" look:
#   - "Premier": Release-level optimization, but deliberately NOT defining
#     NDEBUG, so every #ifndef NDEBUG debug-cheat/level-select shortcut
#     (GM_Level.c, GM_Special.c, GM_Title.c) stays active -- for OBS/YouTube
#     footage where quickly jumping to a specific level/feature to show off
#     matters more than hiding debug-only access.
#   - "Showcase": plain Release build (NDEBUG defined, no debug-cheat
#     access) with the same SSRG-first default -- for release-quality
#     public builds/trailers that don't need cheat access.
# Only takes effect on this variable's FIRST configure in a given build dir,
# same as any other option() default -- use a separate build directory per
# CMAKE_BUILD_TYPE (as this project already does for Debug vs Release).
if(CMAKE_BUILD_TYPE STREQUAL "Premier" OR CMAKE_BUILD_TYPE STREQUAL "Showcase")
  set(SPLASH_DEFAULT ON)
else()
  set(SPLASH_DEFAULT OFF)
endif()
option(SPLASH "Enable the SSRG (Sonic Stuff Research Group) splash screen (for my own demo releases)" ${SPLASH_DEFAULT})

option(SANITIZE "Enable sanitization" OFF)
option(LTO "Enable link-time optimization" OFF)
option(BUILD_TESTS "Build SonicTests (unit tests) and SonicSmoke (headless level-injection smoke test), and register them with ctest" ON)
option(MSVC_LINK_STATIC_RUNTIME
       "Link the static MSVC runtime library (Visual Studio only)" OFF)

#
# Setup #
#

# Define project, source, and includes
project(Sonic LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# All game sources except Main.c (the entry point), so that Main.c's real
# SDL2 entry point and the test suite's own entry point (tests/test_main.c)
# can each link against the exact same compiled game logic as a static lib.
set(
  SONIC_CORE_SOURCES
  # Game
  "src/Constants.h"
  "src/Macros.h"
  "src/Console.c"
  "src/Console.h"
  "src/Kosinski.c"
  "src/Kosinski.h"
  "src/Nemesis.c"
  "src/Nemesis.h"
  "src/MathUtil.c"
  "src/MathUtil.h"
  "src/Game.c"
  "src/Game.h"
  "src/Demo.c"
  "src/Demo.h"
  "src/Video.c"
  "src/Video.h"
  "src/Palette.c"
  "src/Palette.h"
  "src/PLC.c"
  "src/PLC.h"
  "src/Sound.c"
  "src/Sound.h"
  "src/Enigma.c"
  "src/Enigma.h"
  "src/PaletteCycle.c"
  "src/PaletteCycle.h"
  "src/Oscillatory Routines.c"
  "src/Oscillatory Routines.h"
  "src/GM_Sega.c"
  "src/GM_Sega.h"
  "src/GM_Title.c"
  "src/GM_Title.h"
  "src/GM_Level.c"
  "src/GM_Level.h"
  "src/GM_Special.c"
  "src/GM_Special.h"
  "src/Level.c"
  "src/Level.h"
  "src/LevelDraw.c"
  "src/LevelDraw.h"
  "src/LevelScroll.c"
  "src/LevelScroll.h"
  "src/LZWaterFeatures.c"
  "src/LZWaterFeatures.h"
  "src/LevelCollision.c"
  "src/LevelCollision.h"
  "src/SpecialStage.c"
  "src/SpecialStage.h"
  "src/HUD.c"
  "src/HUD.h"
  "src/Object.c"
  "src/Object.h"
  "src/Object/Sonic.c"
  "src/Object/SpecialSonic.c"
  "src/Object/Signpost.c"
  "src/Object/TitleSonic.c"
  "src/Object/PSB.c"
  "src/Object/GHZTunnel.c"
  "src/Object/GHZBridge.c"
  "src/Object/Crabmeat.c"
  "src/Object/HUD.c"
  "src/Object/BuzzBomber.c"
  "src/Object/SwingingPlatform.c"
  "src/Object/BigSpikeBall.c"
  "src/Object/Ring.c"
  "src/Object/Splash.c"
  "src/Object/Splash.h"
  "src/Object/AirBubbles.c"
  "src/Object/AirBubbles.h"
  "src/Object/DrownCount.c"
  "src/Object/DrownCount.h"
  "src/Object/WaterSurface.c"
  "src/Object/WaterSurface.h"
  "src/Object/InvisibleBarrier.c"
  "src/Object/InvisibleBarrier.h"
  "src/Object/DebugList.c"
  "src/Object/DebugList.h"
  "src/Object/Waterfall.c"
  "src/Object/Waterfall.h"
  "src/Object/Monitor.c"
  "src/Object/Explosion.c"
  "src/Object/Checkpoint.c"
  "src/Object/PathSwapper.c"
  "src/Object/PathSwapper.h"
  "src/Object/Chopper.c"
  "src/Object/TitleCard.c"
  "src/Object/Spikes.c"
  "src/Object/ShieldInvincibility.c"
  "src/Object/GameOverCard.c"
  "src/Object/GHZRock.c"
  "src/Object/Motobug.c"
  "src/Object/Spring.c"
  "src/Object/Newtron.c"
  "src/Object/GHZEdge.c"
  "src/Object/Credits.c"
  "src/Object/Animal.c"
  "src/Object/Points.c"
  "src/Object/GiantRing.c"
  "src/Object/GiantRing.h"
  "src/Object/HiddenBonus.c"
  "src/Object/HiddenBonus.h"
  "src/Object/BasicPlatform.c"
  "src/Object/BasicPlatform.h"
  "src/Object/SmashWall.c"
  "src/Object/SmashWall.h"
  "src/Object/Scenery.c"
  "src/Object/Scenery.h"
  "src/Object/CollapseLedge.c"
  "src/Object/CollapseLedge.h"
  "src/Object/CollapseFloor.c"
  "src/Object/VanishingPlatform.c"
  "src/Object/CollapseFloor.h"
  "src/Object/Helix.c"
  "src/Object/Helix.h"
  "src/Object/MarbleBrick.c"
  "src/Object/MarbleBrick.h"
  "src/Object/Button.c"
  "src/Object/Button.h"
  "src/Object/SmashBlock.c"
  "src/Object/SmashBlock.h"
  "src/Object/MovingBlock.c"
  "src/Object/MovingBlock.h"
  "src/Object/LargeGrass.c"
  "src/Object/LargeGrass.h"
  "src/Object/GrassFire.c"
  "src/Object/GrassFire.h"
  "src/Object/LavaTag.c"
  "src/Object/LavaTag.h"
  "src/Object/Caterkiller.c"
  "src/Object/Caterkiller.h"
  "src/Object/LavaMaker.c"
  "src/Object/LavaMaker.h"
  "src/Object/LavaBall.c"
  "src/Object/LavaBall.h"
  "src/Object/GlassBlock.c"
  "src/Object/GlassBlock.h"
  "src/Object/Basaran.c"
  "src/Object/Basaran.h"
  "src/Object/ChainStomp.c"
  "src/Object/ChainStomp.h"
  "src/Object/GeyserMaker.c"
  "src/Object/GeyserMaker.h"
  "src/Object/LavaGeyser.c"
  "src/Object/LavaGeyser.h"
  "src/Object/PushBlock.c"
  "src/Object/PushBlock.h"
  "src/Object/SpinningLight.c"
  "src/Object/SpinningLight.h"
  "src/Object/Bumper.c"
  "src/Object/Bumper.h"
  "src/Object/Roller.c"
  "src/Object/Roller.h"
  "src/Object/SpikeBall.c"
  "src/Object/SpikeBall.h"
  "src/Object/FloatingBlock.c"
  "src/Object/FloatingBlock.h"
  "src/Object/SidewaysStomper.c"
  "src/Object/SidewaysStomper.h"
  "src/Object/LavaWall.c"
  "src/Object/LavaWall.h"
  "src/Object/LZWaterfall.c"
  "src/Object/LZWaterfall.h"
  "src/Object/FlapDoor.c"
  "src/Object/FlapDoor.h"
  "src/Object/Harpoon.c"
  "src/Object/Harpoon.h"
  "src/Object/Pole.c"
  "src/Object/Pole.h"
  "src/Object/Gargoyle.c"
  "src/Object/Gargoyle.h"
  "src/Object/LZBlocks.c"
  "src/Object/LZBlocks.h"
  "src/Object/LZConveyor.c"
  "src/Object/LZConveyor.h"
  "src/Object/Jaws.c"
  "src/Object/Jaws.h"
  "src/Object/Burrobot.c"
  "src/Object/Burrobot.h"
  "src/Object/Orbinaut.c"
  "src/Object/Orbinaut.h"
  "src/Object/Yadrin.c"
  "src/Object/Yadrin.h"
  "src/Object/Bomb.c"
  "src/Object/Bomb.h"
  "src/Object/Elevator.c"
  "src/Object/Elevator.h"
  "src/Object/CirclingPlatform.c"
  "src/Object/CirclingPlatform.h"
  "src/Object/Staircase.c"
  "src/Object/Staircase.h"
  "src/Object/Pylon.c"
  "src/Object/Pylon.h"
  "src/Object/Fan.c"
  "src/Object/Fan.h"
  "src/Object/Seesaw.c"
  "src/Object/Seesaw.h"
  "src/Object/Electrocuter.c"
  "src/Object/Electrocuter.h"
  "src/Object/SmallDoor.c"
  "src/Object/SmallDoor.h"
  "src/Object/SBZConveyor.c"
  "src/Object/SBZConveyor.h"
  "src/Object/Flamethrower.c"
  "src/Object/Flamethrower.h"
  "src/Object/GirderBlock.c"
  "src/Object/GirderBlock.h"
  "src/Object/ScrapStomp.c"
  "src/Object/ScrapStomp.h"
  "src/Object/SpinPlatform.c"
  "src/Object/SpinPlatform.h"
  "src/Object/RunningDisc.c"
  "src/Object/RunningDisc.h"
  "src/Object/RotatingJunction.c"
  "src/Object/RotatingJunction.h"
  "src/Object/Saw.c"
  "src/Object/Saw.h"
  "src/Object/SpinConveyor.c"
  "src/Object/SpinConveyor.h"
  "src/Object/Teleporter.c"
  "src/Object/Teleporter.h"
  "src/Object/BossGreenHill.c"
  "src/Object/BossGreenHill.h"
  "src/Object/BossBall.c"
  "src/Object/BossBall.h"
  "src/Object/BossMarble.c"
  "src/Object/BossMarble.h"
  "src/Object/BossFire.c"
  "src/Object/BossFire.h"
  "src/Object/BossSpringYard.c"
  "src/Object/BossSpringYard.h"
  "src/Object/BossBlock.c"
  "src/Object/BossBlock.h"
  "src/Object/PrisonCapsule.c"
  "src/Object/PrisonCapsule.h"
  "src/Object/VanishSonic.c"
  "src/Object/VanishSonic.h"
  "src/Object/MagicSwitch.c"
  "src/Object/MagicSwitch.h"
  "src/Object/BallHog.c"
  "src/Object/BallHog.h"
  "src/Object/Cannonball.c"
  "src/Object/Cannonball.h"
  # Backend
  "src/Backend/MegaDrive.c"
  "src/Backend/MegaDrive.h"
  "src/Backend/VDP.c"
  "src/Backend/VDP.h"
  "src/Backend/Joypad.c"
  "src/Backend/Joypad.h"
  "src/Backend/SN76489.c"
  "src/Backend/YM2612.h"
  "src/Backend/SN76489.h")

# fmcore (fmcore/fm_voice.h) is the only YM2612.h implementation -- the
# in-house engine, now the settled choice. The ymfm-based alternative
# backend and its verification jigs (FMCoreVerify/FMCoreAlgCheck) have been
# removed; fmcore had enough mileage validated against them that they're no
# longer needed as an ongoing reference.
list(APPEND SONIC_CORE_SOURCES
  "src/Backend/YM2612_FMCore.c"
  "fmcore/fm_operator.c"
  "fmcore/fm_operator.h"
  "fmcore/fm_voice.c"
  "fmcore/fm_voice.h")
set_source_files_properties("fmcore/fm_operator.c" "fmcore/fm_voice.c" "src/Backend/YM2612_FMCore.c" PROPERTIES LANGUAGE C)

add_library(SonicCore STATIC ${SONIC_CORE_SOURCES})

add_executable(Sonic WIN32 "src/Main.c")
target_link_libraries(Sonic PRIVATE SonicCore)

set(ASM
    "Animation/Sonic"
    "Animation/Signpost"
    "Animation/TitleSonic"
    "Animation/PSB"
    "Animation/Crabmeat"
    "Animation/BuzzBomber"
    "Animation/BuzzMissile"
    "Animation/Ring"
    "Animation/Monitor"
    "Animation/Chopper"
    "Animation/ShieldInvincibility"
    "Animation/Motobug"
    "Animation/Spring"
    "Animation/Newtron"
    "Mappings/Sonic"
    "Mappings/SonicDPLC"
    "Mappings/Signpost"
    "Mappings/TitleSonic"
    "Mappings/PSB"
    "Mappings/GHZBridge"
    "Mappings/Crabmeat"
    "Mappings/HUD"
    "Mappings/BuzzBomber"
    "Mappings/BuzzMissile"
    "Mappings/RingREV00"
    "Mappings/RingREV01"
    "Mappings/Monitor"
    "Mappings/Checkpoint"
    "Mappings/PathSwapper"
    "Mappings/InvisibleBarrier"
    "Mappings/LavaTagMarker"
    "Mappings/WaterfallMarker"
    "Mappings/SBZConveyorMarker"
    "Mappings/TeleporterMarker"
    "Mappings/RunningDiscMarker"
    "Mappings/Animals1"
    "Mappings/Animals2"
    "Mappings/Animals3"
    "Mappings/Points"
    "Mappings/Explosion"
    "Mappings/Chopper"
    "Mappings/TitleCard"
    "Mappings/GotThrough"
    "Mappings/SpecialResult"
    "Mappings/Spikes"
    "Mappings/ShieldInvincibility"
    "Mappings/GameOver"
    "Mappings/GHZRock"
    "Mappings/GHZSwing"
    "Mappings/GHZBall"
    "Mappings/SLZSwing"
    "Mappings/BigSpikedBall"
    "Mappings/Motobug"
    "Mappings/Spring"
    "Mappings/Newtron"
    "Mappings/GHZEdge"
    "Mappings/Bumper"
    "Mappings/Credits"
    "Mappings/SSRotate"
    "Mappings/SSGlass"
    "Mappings/SSUp"
    "Mappings/SSDown"
    "Mappings/SSEmerald"
    "Mappings/SSWall"
    "Mappings/Animals1"
    "Mappings/Animals2"
    "Mappings/Animals3"
    "Mappings/SSResultEmerald"
    "Mappings/WaterSurface"
    "Mappings/Bubbles"
    "Mappings/GiantRing"
    "Mappings/RingFlash"
    "Mappings/HiddenBonuses"
    "Mappings/GHZPlatforms"
    "Mappings/SYZPlatforms"
    "Mappings/SLZPlatforms"
    "Mappings/SmashableWalls"
    "Mappings/Scenery"
    "Mappings/CollapsingLedge"
    "Mappings/CollapsingFloors"
    "Mappings/SpikedPoleHelix"
    "Mappings/MZBricks"
    "Mappings/Button"
    "Mappings/SmashableGreenBlock"
    "Mappings/MovingBlocks"
    "Mappings/LZMovingBlocks"
    "Mappings/MZLargeGrassyPlatforms"
    "Mappings/Fireballs"
    "Mappings/LavaTag"
    "Mappings/Caterkiller"
    "Mappings/MZLargeGreenGlassBlocks"
    "Mappings/Basaran"
    "Mappings/ChainedStompers"
    "Mappings/LavaGeyser"
    "Mappings/PushableBlocks"
    "Mappings/Yadrin"
    "Mappings/VanishingPlatforms"
    "Mappings/Light"
    "Mappings/Roller"
    "Mappings/SpikeBallSYZ"
    "Mappings/SpikeBallLZ"
    "Mappings/FloatingBlock"
    "Mappings/SidewaysStomper"
    "Mappings/LavaWall"
    "Mappings/LZWaterfalls"
    "Mappings/FlappingDoor"
    "Mappings/Harpoon"
    "Mappings/PoleThatBreaks"
    "Mappings/Gargoyle"
    "Mappings/LZBlocks"
    "Mappings/LZConveyor"
    "Mappings/Jaws"
    "Mappings/Burrobot"
    "Mappings/Orbinaut"
    "Mappings/Bomb"
    "Mappings/Elevator"
    "Mappings/CirclingPlatform"
    "Mappings/Staircase"
    "Mappings/Pylon"
    "Mappings/Fan"
    "Mappings/Seesaw"
    "Mappings/SeesawBall"
    "Mappings/Electrocuter"
    "Mappings/SmallDoor"
    "Mappings/Flamethrower"
    "Mappings/GirderBlock"
    "Mappings/StomperDoor"
    "Mappings/Trapdoor"
    "Mappings/SpinningPlatforms"
    "Mappings/RunningDisc"
    "Mappings/RotatingJunction"
    "Mappings/SawsPizzaCutters"
    "Mappings/BossItems"
    "Mappings/SYZBossBlocks"
    "Mappings/Eggman"
    "Mappings/PrisonCapsule"
    "Mappings/VanishSonic"
    "Mappings/UnusedSwitch"
    "Mappings/BallHog"
    "Animation/Bomb"
    "Animation/BallHog"
    "Animation/Eggman"
    "Animation/PrisonCapsule"
    "Animation/VanishSonic"
    "Animation/SpinConveyor"
    "Animation/SpinPlatform"
    "Animation/Electrocuter"
    "Animation/SmallDoor"
    "Animation/Flamethrower"
    "Animation/Bumper"
    "Animation/Jaws"
    "Animation/Burrobot"
    "Animation/Orbinaut"
    "Animation/Harpoon"
    "Animation/FlappingDoor"
    "Animation/LavaWall"
    "Animation/LZWaterfalls"
    "Animation/Roller"
    "Animation/LavaGeyser"
    "Animation/Yadrin"
    "Animation/VanishingPlatforms"
    "Animation/Fireballs"
    "Animation/Basaran"
    "Animation/Bubbles"
    "Animation/DrowningCountdown"
    "Animation/GrassFire")

set(RESOURCES
    "Icon"
    "Art/SegaREV00"
    "Art/SegaREV01"
    "Art/JapaneseCredits"
    "Art/TitleFG"
    "Art/Sonic"
    "Art/TitleSonic"
    "Art/TitleTM"
    "Art/TitleCard"
    "Art/GameOver"
    "Art/CreditsFont"
    "Art/Lamppost"
    "Art/HUD"
    "Art/HUDLife"
    "Art/Ring"
    "Art/Points"
    "Art/Monitor"
    "Art/Shield"
    "Art/Invincibility"
    "Art/Explosion"
    "Art/GHZStalk"
    "Art/GHZRock"
    "Art/Crabmeat"
    "Art/BuzzBomber"
    "Art/Chopper"
    "Art/Newtron"
    "Art/Motobug"
    "Art/Spikes"
    "Art/SpringH"
    "Art/SpringV"
    "Art/GHZSwing"
    "Art/GHZBridge"
    "Art/GHZLog"
    "Art/GHZBall"
    "Art/GHZWall1"
    "Art/GHZWall2"
    "Art/LZBlock1"
    "Art/LZBlock2"
    "Art/LZBlock3"
    "Art/Splash"
    "Art/Water"
    "Art/LZSpikeBall"
    "Art/FlapDoor"
    "Art/Bubbles"
    "Art/LZDoor1"
    "Art/LZDoor2"
    "Art/Harpoon"
    "Art/Burrobot"
    "Art/LZPole"
    "Art/LZWheel"
    "Art/Gargoyle"
    "Art/LZSonic"
    "Art/LZPlatfm"
    "Art/Orbinaut"
    "Art/Jaws"
    "Art/LZSwitch"
    "Art/Cork"
    "Art/MZMetal"
    "Art/MZFire"
    "Art/MZGlass"
    "Art/Lava"
    "Art/Yadrin"
    "Art/Basaran"
    "Art/Caterkiller"
    "Art/MZSwitch"
    "Art/MZBlock"
    "Art/Bomb"
    "Art/SLZBlock"
    "Art/SLZWall"
    "Art/Seesaw"
    "Art/Fan"
    "Art/Pylon"
    "Art/SLZSwing"
    "Art/SLZCannon"
    "Art/SLZSpike"
    "Art/Bumper"
    "Art/Roller"
    "Art/SYZSpike1"
    "Art/SYZSpike2"
    "Art/GHZ1"
    "Art/GHZ2"
    "Art/LZ"
    "Art/MZ"
    "Art/SLZ"
    "Art/SYZ"
    "Art/SBZ"
    "Art/Signpost"
    "Art/HiddenBonus"
    "Art/BigFlash"
    "Art/BigRing"
    "Art/GHZWaterfall"
    "Art/GHZFlowerLarge"
    "Art/GHZFlowerSmall"
    "Art/Stomper"
    "Art/SBZDoor1"
    "Art/SBZDoor2"
    "Art/Girder"
    "Art/Ballhog"
    "Art/SBZWheel1"
    "Art/SBZWheel2"
    "Art/Cutter"
    "Art/FlamePipe"
    "Art/SBZFloor"
    "Art/SBZBlock"
    "Art/SlideFloor"
    "Art/Electric"
    "Art/TrapDoor"
    "Art/SPinPform"
    "Art/MZLava1"
    "Art/MZLava2"
    "Art/MZTorch"
    "Art/SBZSmoke"
    "Art/HUDNum"
    "Art/LifeNum"
    "Art/Text"
    "Art/SSClouds"
    "Art/SSBack"
    "Art/SSWall"
    "Art/SSGoal"
    "Art/SSSpeed"
    "Art/SSRotate"
    "Art/SSLife"
    "Art/SSTwinkle"
    "Art/SSChecker"
    "Art/SSGhost"
    "Art/SSWarp"
    "Art/SSGlass"
    "Art/SSEmerald"
    "Art/SSZone1"
    "Art/SSZone2"
    "Art/SSZone3"
    "Art/SSZone4"
    "Art/SSZone5"
    "Art/SSZone6"
    "Art/Rabbit"
    "Art/Flicky"
    "Art/Penguin"
    "Art/Seal"
    "Art/Squirrel"
    "Art/Pig"
    "Art/Chicken"
    "Art/SSResultEmerald"
    "Art/Eggman"
    "Art/Weapons"
    "Art/Prison"
    "Art/Exhaust"
    "Art/ResultEm"
    "Art/MiniSonic"
    "Art/Warp"
    "Art/EndFlower"
    "Art/EndEm"
    "Art/EndEggman"
    "Art/EndStH"
    "Art/EndSonic"
    "Art/TryAgain"
    "Art/CreditText"
    "Art/FZEggman"
    "Art/FZBoss"
    "Art/SBZ2Eggman"
    "Tilemap/SegaREV00"
    "Tilemap/SegaREV01"
    "Tilemap/JapaneseCredits"
    "Tilemap/TitleFG"
    "Palette/SegaBG"
    "Palette/Title"
    "Palette/LevelSel"
    "Palette/Sonic"
    "Palette/GHZ"
    "Palette/LZ"
    "Palette/MZ"
    "Palette/SYZ"
    "Palette/SLZ"
    "Palette/SBZ1"
    "Palette/Special"
    "Palette/LZWater"
    "Palette/SBZ3"
    "Palette/SBZ3Water"
    "Palette/SBZ2"
    "Palette/SonicLZ"
    "Palette/SonicSBZ"
    "Palette/SSResults"
    "Palette/Continue"
    "Palette/Ending"
    "Palette/Sega1"
    "Palette/Sega2"
    "Palette/TitleCycle"
    "Palette/GHZCycle"
    "Palette/LZCycWaterfall"
    "Palette/LZCycConveyor"
    "Palette/LZCycConveyorUW"
    "Palette/SBZ3CycWaterfall"
    "Palette/SLZCycle"
    "Palette/SYZCycle1"
    "Palette/SYZCycle2"
    "Palette/SBZCyc1"
    "Palette/SBZCyc2"
    "Palette/SBZCyc3"
    "Palette/SBZCyc4"
    "Palette/SBZCyc5"
    "Palette/SBZCyc6"
    "Palette/SBZCyc7"
    "Palette/SBZCyc8"
    "Palette/SBZCyc9"
    "Palette/SBZCyc10"
    "Palette/SSCyc1"
    "Palette/SSCyc2"
    ${ASM}
    "Mappings/ExplodeBomb"
    "Layout/GHZ1"
    "Layout/GHZ2"
    "Layout/GHZ3"
    "Layout/LZ1"
    "Layout/LZ2"
    "Layout/LZ3"
    "Layout/MZ1"
    "Layout/MZ2"
    "Layout/MZ3"
    "Layout/SLZ1"
    "Layout/SLZ2"
    "Layout/SLZ3"
    "Layout/SYZ1"
    "Layout/SYZ2"
    "Layout/SYZ3"
    "Layout/SBZ1"
    "Layout/SBZ2"
    "Layout/SBZ3"
    "Layout/Ending"
    "ObjectLayout/GHZ1"
    "ObjectLayout/GHZ2"
    "ObjectLayout/GHZ3REV00"
    "ObjectLayout/GHZ3REV01"
    "ObjectLayout/LZ1PF1"
    "ObjectLayout/LZ1PF2"
    "ObjectLayout/LZ1REV00"
    "ObjectLayout/LZ1REV01"
    "ObjectLayout/LZ2"
    "ObjectLayout/LZ2PF1"
    "ObjectLayout/LZ2PF2"
    "ObjectLayout/LZ3PF1"
    "ObjectLayout/LZ3PF2"
    "ObjectLayout/LZ3REV00"
    "ObjectLayout/LZ3REV01"
    "ObjectLayout/MZ1REV00"
    "ObjectLayout/MZ1REV01"
    "ObjectLayout/MZ2"
    "ObjectLayout/MZ3"
    "ObjectLayout/SLZ1"
    "ObjectLayout/SLZ2"
    "ObjectLayout/SLZ3"
    "ObjectLayout/SYZ1"
    "ObjectLayout/SYZ2"
    "ObjectLayout/SYZ3REV00"
    "ObjectLayout/SYZ3REV01"
    "ObjectLayout/SBZ1PF1"
    "ObjectLayout/SBZ1PF2"
    "ObjectLayout/SBZ1PF3"
    "ObjectLayout/SBZ1PF4"
    "ObjectLayout/SBZ1PF5"
    "ObjectLayout/SBZ1PF6"
    "ObjectLayout/SBZ1REV00"
    "ObjectLayout/SBZ1REV01"
    "ObjectLayout/SBZ2"
    "ObjectLayout/SBZ3"
    "ObjectLayout/FZ"
    "ObjectLayout/Ending"
    "Map16/GHZ"
    "Map16/LZ"
    "Map16/MZ"
    "Map16/SBZ"
    "Map16/SLZ"
    "Map16/SYZ"
    "Map128/GHZ"
    "Map128/LZ"
    "Map128/MZREV00"
    "Map128/MZREV01"
    "Map128/SBZREV00"
    "Map128/SBZREV01"
    "Map128/SLZ"
    "Map128/SYZ"
    "Collision/GHZ1"
    "Collision/GHZ2"
    "Collision/LZ1"
    "Collision/LZ2"
    "Collision/MZ1"
    "Collision/MZ2"
    "Collision/SLZ1"
    "Collision/SLZ2"
    "Collision/SYZ1"
    "Collision/SYZ2"
    "Collision/SBZ1"
    "Collision/SBZ2"
    "Collision/Angle"
    "Collision/HeightMap"
    "Collision/WidthMap"
    "Demo/IntroGHZ"
    "Demo/IntroMZ"
    "Demo/IntroSYZ"
    "Demo/IntroSS"
    "Demo/EndingGHZ1"
    "Demo/EndingMZ"
    "Demo/EndingSYZ"
    "Demo/EndingLZ"
    "Demo/EndingSLZ"
    "Demo/EndingSBZ1"
    "Demo/EndingSBZ2"
    "Demo/EndingGHZ2"
    "SSLayout/1"
    "SSLayout/2"
    "SSLayout/3"
    "SSLayout/4"
    "SSLayout/5REV00"
    "SSLayout/5REV01"
    "SSLayout/6REV00"
    "SSLayout/6REV01"
    "DAC/kick"
    "DAC/snare"
    "DAC/timpani"
    "DAC/scratch"
    "DAC/clap"
    "DAC/tom"
    "DAC/bongo"
    "DAC/s3snare"
    "DAC/s3tom"
    "DAC/s3kick"
    "DAC/muffled_snare"
    "DAC/crash"
    "DAC/ride"
    "DAC/metal_hit_1"
    "DAC/metal_hit_2"
    "DAC/metal_hit_3"
    "DAC/s3clap"
    "DAC/etom"
    "DAC/pitched_snare"
    "DAC/s3timpani"
    "DAC/quick_loose_snare"
    "DAC/click"
    "DAC/power_kick"
    "DAC/quick_glass_crash"
    "DAC/glass_crash_snare"
    "DAC/glass_crash"
    "DAC/glass_crash_kick"
    "DAC/quiet_glass_crash"
    "DAC/snare_kick"
    "DAC/xbass_kick"
    "DAC/dance_snare"
    "DAC/loose_kick"
    "DAC/mod_loose_kick"
    "DAC/wood_block"
    "DAC/wood_block_2"
    "DAC/conga"
    "DAC/hit_drum_1"
    "DAC/hit_drum_2"
    "DAC/hit_drum_3"
    "DAC/metal_crash_hit"
    "DAC/low_echo_clap"
    "DAC/hiphop_kick"
    "DAC/dance_kick"
    "DAC/hiphop_kick_2"
    "DAC/hiphop_kick_3"
    "DAC/deep_hit"
    "DAC/wood_block_3"
    "DAC/unused"
    "DAC/reverse"
    "DAC/psytrance_kick"
    "DAC/psytrance_snare"
    "DAC/cowbell"
    "DAC/rimshot"
    "DAC/cuica"
    "DAC/guiro"
    "PCM/sega"
    "PSG/psg1"
    "PSG/psg2"
    "PSG/psg3"
    "PSG/psg4"
    "PSG/psg5"
    "PSG/psg6"
    "PSG/psg7"
    "PSG/psg8"
    "PSG/psg9")

target_include_directories(SonicCore PUBLIC "src")

# ##############################################################################
# Compiler flags #
# ##############################################################################

# Revision
if(REV01)
  target_compile_definitions(SonicCore PUBLIC SCP_REV01)
else()
  target_compile_definitions(SonicCore PUBLIC SCP_REV00)
endif()

# Region
if(JAPANESE)
  target_compile_definitions(SonicCore PUBLIC SCP_JP)
endif()

# Bug fixes
if(FIX_BUGS)
  target_compile_definitions(SonicCore PUBLIC SCP_FIX_BUGS)
endif()

# Sanitization
if(SANITIZE)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Og -ggdb3 -fsanitize=address")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address")
endif()

# Splash
if(SPLASH)
  target_compile_definitions(SonicCore PUBLIC SCP_SPLASH)
  target_sources(SonicCore PRIVATE "src/GM_SSRG.c" "src/GM_SSRG.h" "src/GM_Countdown.c" "src/GM_Countdown.h")
  list(
    APPEND
    RESOURCES
    "SSRG/ArtLink"
    "SSRG/ArtMain"
    "SSRG/ArtSonic"
    "SSRG/ArtSquare"
    "SSRG/MapLink"
    "SSRG/MapMain"
    "SSRG/MapSquare")
endif()

# Strip release builds
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -s")

# Premier: same optimization+strip as Release, but deliberately no -DNDEBUG
# (unlike CMake's built-in Release/MinSizeRel/RelWithDebInfo, which all add
# it) -- see the SPLASH_DEFAULT comment above for why: this build type wants
# every #ifndef NDEBUG debug-cheat path to stay compiled in and active.
set(CMAKE_C_FLAGS_PREMIER
    "-O3 -s"
    CACHE STRING "Flags used by the C compiler for the Premier build type" FORCE)
set(CMAKE_CXX_FLAGS_PREMIER
    "-O3 -s"
    CACHE STRING "Flags used by the CXX compiler for the Premier build type" FORCE)
set(CMAKE_EXE_LINKER_FLAGS_PREMIER
    ""
    CACHE STRING "Flags used by the linker for the Premier build type" FORCE)
mark_as_advanced(CMAKE_C_FLAGS_PREMIER CMAKE_CXX_FLAGS_PREMIER CMAKE_EXE_LINKER_FLAGS_PREMIER)

# Showcase: identical flags to Release (optimization, strip, AND -DNDEBUG --
# unlike Premier, this keeps debug-cheat paths compiled out) -- exists as its
# own build type purely to get the SPLASH-on-by-default behavior above and
# land in its own bin/Showcase/ directory, without affecting plain Release.
set(CMAKE_C_FLAGS_SHOWCASE
    "-O3 -DNDEBUG -s"
    CACHE STRING "Flags used by the C compiler for the Showcase build type" FORCE)
set(CMAKE_CXX_FLAGS_SHOWCASE
    "-O3 -DNDEBUG -s"
    CACHE STRING "Flags used by the CXX compiler for the Showcase build type" FORCE)
set(CMAKE_EXE_LINKER_FLAGS_SHOWCASE
    ""
    CACHE STRING "Flags used by the linker for the Showcase build type" FORCE)
mark_as_advanced(CMAKE_C_FLAGS_SHOWCASE CMAKE_CXX_FLAGS_SHOWCASE CMAKE_EXE_LINKER_FLAGS_SHOWCASE)

# Force warnings
if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  target_compile_options(SonicCore PRIVATE /W4)
else()
  target_compile_options(SonicCore PRIVATE -Wall -Wextra -pedantic)
endif()

# Use LTO if requested
if(LTO)
  include(CheckIPOSupported)
  check_ipo_supported(RESULT result)
  if(result)
    set_target_properties(Sonic SonicCore PROPERTIES INTERPROCEDURAL_OPTIMIZATION
                                               TRUE)
  endif()
endif()

# ##############################################################################
# Other compile-time defines #
# ##############################################################################

# Determine endianness
include(TestBigEndian)
test_big_endian(ENDIAN)
if(ENDIAN)
  target_compile_definitions(SonicCore PUBLIC SCP_BIG_ENDIAN)
else()
  target_compile_definitions(SonicCore PUBLIC SCP_LIL_ENDIAN)
endif()

#
# MSVC #
#

# This is messy as hell, and has been replaced by CMAKE_MSVC_RUNTIME_LIBRARY,
# but that's a very recent CMake addition, so we're still doing it this way for
# now
if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC" AND MSVC_LINK_STATIC_RUNTIME)
  # Statically-link the CRT (vcpkg static libs do this)
  foreach(flag_var CMAKE_C_FLAGS CMAKE_C_FLAGS_DEBUG CMAKE_C_FLAGS_RELEASE
                   CMAKE_C_FLAGS_MINSIZEREL CMAKE_C_FLAGS_RELWITHDEBINFO)
    if(${flag_var} MATCHES "/MD")
      string(REGEX REPLACE "/MD" "/MT" ${flag_var} "${${flag_var}}")
    endif()
  endforeach()
endif()

# Do some other MSVC fixes
if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  # Disable warnings that normally fire up on MSVC when using "unsafe" functions
  # instead of using MSVC's "safe" _s functions
  target_compile_definitions(SonicCore PUBLIC _CRT_SECURE_NO_WARNINGS)

  # Make it so source files are recognized as UTF-8 by MSVC
  target_compile_options(SonicCore PRIVATE "/utf-8")

  # Use `main` instead of `WinMain`
  set_target_properties(Sonic PROPERTIES LINK_FLAGS "/ENTRY:mainCRTStartup")
endif()

# ##############################################################################
# Further compiler setup #
# ##############################################################################

# Force strict C99
set_target_properties(
  SonicCore Sonic
  PROPERTIES C_STANDARD 99
             C_STANDARD_REQUIRED ON
             C_EXTENSIONS OFF)

# Set output directory
set(BUILD_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin")

# Sonic lands in bin/<config>/Sonic (bin/Debug/Sonic, bin/Release/Sonic, ...)
# -- $<CONFIG> resolves correctly for both single-config generators (Unix
# Makefiles, using CMAKE_BUILD_TYPE) and multi-config ones (Ninja Multi-
# Config, Visual Studio), unlike the old fixed RUNTIME_OUTPUT_DIRECTORY_*
# per-config properties, which only multi-config generators actually read.
set_target_properties(Sonic PROPERTIES RUNTIME_OUTPUT_DIRECTORY
                                        "${BUILD_DIRECTORY}/$<CONFIG>")

# ##############################################################################
# Backends #
# ##############################################################################

# Find PkgConfig for dependency linking
find_package(PkgConfig QUIET)

if(BACKEND MATCHES "SDL2")
  target_compile_definitions(SonicCore PUBLIC SCP_BACKEND_SDL2)
  target_sources(
    SonicCore PRIVATE "src/Backend/SDL2/System.c" "src/Backend/SDL2/Render.c"
                      "src/Backend/SDL2/Input.c" "src/Backend/SDL2/Audio.c")

  # Compile and link SDL2
  if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    set(LIBC
        ON
        CACHE INTERNAL "" FORCE) # Needed to prevent possible 'symbol already
                                 # defined' errors
  endif()

  # 1. Look for a SDL2 package,
  # 2. look for the SDL2 component and
  # 3. fail if none can be found
  find_package(SDL2 REQUIRED CONFIG REQUIRED COMPONENTS SDL2)

  # SDL2::SDL2main may or may not be available. It is e.g. required by Windows
  # GUI applications. Only the real Sonic executable needs it -- SonicTests
  # has its own plain console main() and must not get SDL's main-shimming.
  if(TARGET SDL2::SDL2main)

    # It has an implicit dependency on SDL2 functions, so it MUST be added
    # before SDL2::SDL2 (or SDL2::SDL2-static)
    target_link_libraries(Sonic PRIVATE SDL2::SDL2main)
  endif()

  # Link to the actual SDL2 library. SDL2::SDL2 is the shared SDL library,
  # SDL2::SDL2-static is the static SDL libarary. PUBLIC so it also propagates
  # to whatever links SonicCore (Sonic itself, and SonicTests).
  target_link_libraries(SonicCore PUBLIC SDL2::SDL2)
endif()

# libm -- Sound.c computes PSG note periods from note frequencies (pow/exp2)
if(NOT WIN32)
  target_link_libraries(SonicCore PUBLIC m)
endif()

# ##############################################################################
# Resource conversion #
# ##############################################################################

# Build bin2h externally, so it isn't cross-compiled when Sonic is
# (Emscripten, cross-GCC, MinGW on Linux, etc.)
include(ExternalProject)

ExternalProject_Add(
  bin2h
  SOURCE_DIR "${CMAKE_SOURCE_DIR}/tools/bin2h"
  DOWNLOAD_COMMAND ""
  UPDATE_COMMAND ""
  BUILD_BYPRODUCTS "<INSTALL_DIR>/bin/bin2h"
  CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR> -DCMAKE_BUILD_TYPE=Release
  INSTALL_COMMAND ${CMAKE_COMMAND} --build . --config Release --target install)

ExternalProject_Get_Property(bin2h INSTALL_DIR)

add_executable(bin2h_tool IMPORTED)
add_dependencies(bin2h_tool bin2h)
set_target_properties(bin2h_tool PROPERTIES IMPORTED_LOCATION
                                            "${INSTALL_DIR}/bin/bin2h")

# Convert resources to header files
foreach(FILENAME IN LISTS RESOURCES)
  set(IN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/res")
  set(OUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/Resource")
  get_filename_component(DIRECTORY "${FILENAME}" DIRECTORY)
  # Safety margin: Nemesis/Kosinski/Enigma decompression speculatively reads
  # a byte or two past the logical end of their compressed input as an
  # artifact of their bit-refill logic. Harmless on real hardware
  # (contiguous ROM) but an actual out-of-bounds read here, since each
  # resource is its own fixed-size array. Map16_* (see src/Enigma.c) used to
  # be an exception to this -- pre-decompressed and padding-free, since
  # sizeof() on it was relied on elsewhere -- back when it stored plain
  # decompressed data instead of genuine compressed Enigma data.
  set(PAD_BYTES 8)
  add_custom_command(
    OUTPUT "${OUT_DIR}/${FILENAME}.h"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${OUT_DIR}/${DIRECTORY}"
    COMMAND bin2h_tool "${IN_DIR}/${FILENAME}" "${OUT_DIR}/${FILENAME}.h" ${PAD_BYTES}
    DEPENDS bin2h_tool "${IN_DIR}/${FILENAME}")
  target_sources(SonicCore PRIVATE "${OUT_DIR}/${FILENAME}.h")
endforeach()

include(ExternalProject)

ExternalProject_Add(
  clownassembler
  SOURCE_DIR "${CMAKE_SOURCE_DIR}/contrib/clownassembler"
  DOWNLOAD_COMMAND ""
  UPDATE_COMMAND ""
  BUILD_BYPRODUCTS "<INSTALL_DIR>/bin/clownassembler"
  CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR> -DCMAKE_BUILD_TYPE=Release
  INSTALL_COMMAND ${CMAKE_COMMAND} --build . --config Release --target install)

ExternalProject_Get_Property(clownassembler INSTALL_DIR)

add_executable(clownassembler_tool IMPORTED)
add_dependencies(clownassembler_tool clownassembler)
set_target_properties(
  clownassembler_tool PROPERTIES IMPORTED_LOCATION
                                 "${INSTALL_DIR}/bin/clownassembler")

# Assemble asm code
foreach(FILENAME IN LISTS ASM)
  set(IN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/asm")
  set(OUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/res")
  get_filename_component(DIRECTORY "${FILENAME}.asm" DIRECTORY)
  add_custom_command(
    OUTPUT "${OUT_DIR}/${FILENAME}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${OUT_DIR}/${DIRECTORY}"
    COMMAND clownassembler_tool -i "${IN_DIR}/${FILENAME}.asm" -o
            "${OUT_DIR}/${FILENAME}"
    DEPENDS clownassembler_tool "${IN_DIR}/${FILENAME}.asm")
  target_sources(SonicCore PRIVATE "${OUT_DIR}/${FILENAME}")
endforeach()

# ##############################################################################
# ParadoxSMPS (JSON song/SFX format) #
# ##############################################################################
# libparadoxsmps (libparadoxsmps/json.c + compiler.c) is the ONE canonical
# ParadoxSMPS compiler -- both this real build (via the jsonc2h_tool CLI
# below) and ParadoxComposer (linked directly) use the exact same code, so
# there is no second hand-ported implementation left to drift out of sync
# (the retired smps2asmc/.asm pipeline below this comment used to be the
# real build's source of truth; ParadoxComposer separately carried its own
# Qt/C++ port -- "a C based converter and compiler can be hooked into by the
# daw directly and guarantee the same code quality consistently").
#
# jsonc2h compiles one .jsonc straight to a header in a single step -- no
# intermediate .asm, no generated-C song-builder program to build and run,
# unlike the old three-step translate/build/run pipeline. Same relationship
# moc has to a Qt header.
add_library(paradoxsmps STATIC
  "libparadoxsmps/json.c"
  "libparadoxsmps/json.h"
  "libparadoxsmps/compiler.c"
  "libparadoxsmps/compiler.h"
  "libparadoxsmps/converter.c"
  "libparadoxsmps/converter.h")
set_target_properties(paradoxsmps PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)

# SonicCore links this directly now too -- the SMPS runtime engine (Sound.c)
# walks PJValue JSON trees straight at playback time (see the "SMPS runtime:
# walk JSON directly" plan) instead of a precompiled byte stream, so the
# actual game binary needs the JSON value model at runtime, not just the
# offline compiler tools.
target_link_libraries(SonicCore PUBLIC paradoxsmps)
target_include_directories(SonicCore PUBLIC "${CMAKE_SOURCE_DIR}/libparadoxsmps")

add_executable(jsonc2h_tool "libparadoxsmps/jsonc2h_main.c")
target_link_libraries(jsonc2h_tool PRIVATE paradoxsmps)
target_include_directories(jsonc2h_tool PRIVATE "${CMAKE_SOURCE_DIR}/libparadoxsmps")
set_target_properties(jsonc2h_tool PROPERTIES OUTPUT_NAME jsonc2h)

# jsonc2rawh: companion to jsonc2h_tool above, but embeds a song's own
# SOURCE TEXT (not compiled bytes) as a C string -- for the JSON
# tree-walking runtime engine, which parses and plays a song's PJValue
# tree directly rather than a compiled byte stream.
add_executable(jsonc2rawh_tool "libparadoxsmps/jsonc2rawh_main.c")
target_link_libraries(jsonc2rawh_tool PRIVATE paradoxsmps)
target_include_directories(jsonc2rawh_tool PRIVATE "${CMAKE_SOURCE_DIR}/libparadoxsmps")
set_target_properties(jsonc2rawh_tool PROPERTIES OUTPUT_NAME jsonc2rawh)

# asm2json: migration tool (not part of the game build), converts a real
# SMPS2ASM .asm song/SFX file to ParadoxSMPS JSONC -- see converter.c's own
# comment. Kept for re-conversion/verification against
# tools/paradoxsmps/asm_to_json.py, the same relationship jsonc2h_tool has
# to json_to_header.py.
add_executable(asm2json_tool "libparadoxsmps/asm2json_main.c")
target_link_libraries(asm2json_tool PRIVATE paradoxsmps)
target_include_directories(asm2json_tool PRIVATE "${CMAKE_SOURCE_DIR}/libparadoxsmps")
set_target_properties(asm2json_tool PROPERTIES OUTPUT_NAME asm2json)

# Compiles one already-converted ParadoxSMPS .jsonc file, adding its
# resulting resource header to SonicCore. JSONC_FILE is relative to
# tools/paradoxsmps/converted/ (e.g. "music/Mus81 - GHZ.jsonc"); ARRAY_NAME
# becomes both the generated C array's name and the output header's
# filename (Resource/Music/${ARRAY_NAME}.h).
function(add_paradoxsmps_song JSONC_FILE ARRAY_NAME)
  set(IN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/tools/paradoxsmps/converted")
  set(OUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/Resource/Music")
  set(OUT_H "${OUT_DIR}/${ARRAY_NAME}.h")

  add_custom_command(
    OUTPUT "${OUT_H}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${OUT_DIR}"
    COMMAND jsonc2h_tool "${IN_DIR}/${JSONC_FILE}" "${OUT_H}" "${ARRAY_NAME}"
    DEPENDS jsonc2h_tool "${IN_DIR}/${JSONC_FILE}")

  target_sources(SonicCore PRIVATE "${OUT_H}")

  # Companion raw-JSON-text header, alongside the compiled-bytes one above
  # -- Sound.c's sound_table_json[] (parallel to sound_table[]) includes
  # these so the sound test (and anything else, eventually) can play a
  # song through the JSON engine instead of the byte-VM.
  set(OUT_JSON_H "${OUT_DIR}/${ARRAY_NAME}_json.h")
  add_custom_command(
    OUTPUT "${OUT_JSON_H}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${OUT_DIR}"
    COMMAND jsonc2rawh_tool "${IN_DIR}/${JSONC_FILE}" "${OUT_JSON_H}" "${ARRAY_NAME}"
    DEPENDS jsonc2rawh_tool "${IN_DIR}/${JSONC_FILE}")

  target_sources(SonicCore PRIVATE "${OUT_JSON_H}")
endfunction()

# Full music/SFX catalog. res/Music/ also contains "E0 - Spin Dash Rev.asm"
# (and its converted/ counterpart, if any), deliberately NOT wired up here:
# it's a SonicDriverVer=2 test file, and this compiler only implements
# driver 1 (matching the original smps2asmc/smps.c pipeline's own scope).
add_paradoxsmps_song("music/Mus81 - GHZ.jsonc" Mus81_GHZ)
add_paradoxsmps_song("music/Mus82 - LZ.jsonc" Mus82_LZ)
add_paradoxsmps_song("music/Mus83 - MZ.jsonc" Mus83_MZ)
add_paradoxsmps_song("music/Mus84 - SLZ.jsonc" Mus84_SLZ)
add_paradoxsmps_song("music/Mus85 - SYZ.jsonc" Mus85_SYZ)
add_paradoxsmps_song("music/Mus86 - SBZ.jsonc" Mus86_SBZ)
add_paradoxsmps_song("music/Mus87 - Invincibility.jsonc" Mus87_Invincibility)
add_paradoxsmps_song("music/Mus88 - Extra Life.jsonc" Mus88_Extra_Life)
add_paradoxsmps_song("music/Mus89 - Special Stage.jsonc" Mus89_Special_Stage)
add_paradoxsmps_song("music/Mus8A - Title Screen.jsonc" Mus8A_Title_Screen)
add_paradoxsmps_song("music/Mus8B - Ending.jsonc" Mus8B_Ending)
add_paradoxsmps_song("music/Mus8C - Boss.jsonc" Mus8C_Boss)
add_paradoxsmps_song("music/Mus8D - FZ.jsonc" Mus8D_FZ)
add_paradoxsmps_song("music/Mus8E - Sonic Got Through.jsonc" Mus8E_Sonic_Got_Through)
add_paradoxsmps_song("music/Mus8F - Game Over.jsonc" Mus8F_Game_Over)
add_paradoxsmps_song("music/Mus90 - Continue Screen.jsonc" Mus90_Continue_Screen)
add_paradoxsmps_song("music/Mus91 - Credits.jsonc" Mus91_Credits)
add_paradoxsmps_song("music/Mus92 - Drowning.jsonc" Mus92_Drowning)
add_paradoxsmps_song("music/Mus93 - Get Emerald.jsonc" Mus93_Get_Emerald)

add_paradoxsmps_song("sfx/SndA0 - Jump.jsonc" SndA0_Jump)
add_paradoxsmps_song("sfx/SndA1 - Lamppost.jsonc" SndA1_Lamppost)
add_paradoxsmps_song("sfx/SndA2.jsonc" SndA2)
add_paradoxsmps_song("sfx/SndA3 - Death.jsonc" SndA3_Death)
add_paradoxsmps_song("sfx/SndA4 - Skid.jsonc" SndA4_Skid)
add_paradoxsmps_song("sfx/SndA5.jsonc" SndA5)
add_paradoxsmps_song("sfx/SndA6 - Hit Spikes.jsonc" SndA6_Hit_Spikes)
add_paradoxsmps_song("sfx/SndA7 - Push Block.jsonc" SndA7_Push_Block)
add_paradoxsmps_song("sfx/SndA8 - SS Goal.jsonc" SndA8_SS_Goal)
add_paradoxsmps_song("sfx/SndA9 - SS Item.jsonc" SndA9_SS_Item)
add_paradoxsmps_song("sfx/SndAA - Splash.jsonc" SndAA_Splash)
add_paradoxsmps_song("sfx/SndAB.jsonc" SndAB)
add_paradoxsmps_song("sfx/SndAC - Hit Boss.jsonc" SndAC_Hit_Boss)
add_paradoxsmps_song("sfx/SndAD - Get Bubble.jsonc" SndAD_Get_Bubble)
add_paradoxsmps_song("sfx/SndAE - Fireball.jsonc" SndAE_Fireball)
add_paradoxsmps_song("sfx/SndAF - Shield.jsonc" SndAF_Shield)
add_paradoxsmps_song("sfx/SndB0 - Saw.jsonc" SndB0_Saw)
add_paradoxsmps_song("sfx/SndB1 - Electric.jsonc" SndB1_Electric)
add_paradoxsmps_song("sfx/SndB2 - Drown Death.jsonc" SndB2_Drown_Death)
add_paradoxsmps_song("sfx/SndB3 - Flamethrower.jsonc" SndB3_Flamethrower)
add_paradoxsmps_song("sfx/SndB4 - Bumper.jsonc" SndB4_Bumper)
add_paradoxsmps_song("sfx/SndB5 - Ring.jsonc" SndB5_Ring)
add_paradoxsmps_song("sfx/SndB6 - Spikes Move.jsonc" SndB6_Spikes_Move)
add_paradoxsmps_song("sfx/SndB7 - Rumbling.jsonc" SndB7_Rumbling)
add_paradoxsmps_song("sfx/SndB8.jsonc" SndB8)
add_paradoxsmps_song("sfx/SndB9 - Collapse.jsonc" SndB9_Collapse)
add_paradoxsmps_song("sfx/SndBA - SS Glass.jsonc" SndBA_SS_Glass)
add_paradoxsmps_song("sfx/SndBB - Door.jsonc" SndBB_Door)
add_paradoxsmps_song("sfx/SndBC - Teleport.jsonc" SndBC_Teleport)
add_paradoxsmps_song("sfx/SndBD - ChainStomp.jsonc" SndBD_ChainStomp)
add_paradoxsmps_song("sfx/SndBE - Roll.jsonc" SndBE_Roll)
add_paradoxsmps_song("sfx/SndBF - Get Continue.jsonc" SndBF_Get_Continue)
add_paradoxsmps_song("sfx/SndC0 - Basaran Flap.jsonc" SndC0_Basaran_Flap)
add_paradoxsmps_song("sfx/SndC1 - Break Item.jsonc" SndC1_Break_Item)
add_paradoxsmps_song("sfx/SndC2 - Drown Warning.jsonc" SndC2_Drown_Warning)
add_paradoxsmps_song("sfx/SndC3 - Giant Ring.jsonc" SndC3_Giant_Ring)
add_paradoxsmps_song("sfx/SndC4 - Bomb.jsonc" SndC4_Bomb)
add_paradoxsmps_song("sfx/SndC5 - Cash Register.jsonc" SndC5_Cash_Register)
add_paradoxsmps_song("sfx/SndC6 - Ring Loss.jsonc" SndC6_Ring_Loss)
add_paradoxsmps_song("sfx/SndC7 - Chain Rising.jsonc" SndC7_Chain_Rising)
add_paradoxsmps_song("sfx/SndC8 - Burning.jsonc" SndC8_Burning)
add_paradoxsmps_song("sfx/SndC9 - Hidden Bonus.jsonc" SndC9_Hidden_Bonus)
add_paradoxsmps_song("sfx/SndCA - Enter SS.jsonc" SndCA_Enter_SS)
add_paradoxsmps_song("sfx/SndCB - Wall Smash.jsonc" SndCB_Wall_Smash)
add_paradoxsmps_song("sfx/SndCC - Spring.jsonc" SndCC_Spring)
add_paradoxsmps_song("sfx/SndCD - Switch.jsonc" SndCD_Switch)
add_paradoxsmps_song("sfx/SndCE - Ring Left Speaker.jsonc" SndCE_Ring_Left_Speaker)
add_paradoxsmps_song("sfx/SndCF - Signpost.jsonc" SndCF_Signpost)
add_paradoxsmps_song("sfx/SndD0 - Waterfall.jsonc" SndD0_Waterfall)

if(SPLASH)
  add_paradoxsmps_song("music/Mus94 - SSRG.jsonc" Mus94_SSRG)
endif()

# ##############################################################################
# Tests #
# ##############################################################################

if(BUILD_TESTS)
  enable_testing()

  # Showcase only wants the tools relevant to producing/recording pre-scripted
  # footage (SonicSmoke to play back scripted movement, SonicDemoRecord to
  # capture it) plus the main Sonic binary itself -- not the rest of the
  # developer-diagnostic toolkit (unit tests, joypad/sound-trace tools, the
  # FM voice editor, etc), which have no role in producing showcase footage.
  if(CMAKE_BUILD_TYPE STREQUAL "Showcase")
    set(SONIC_FULL_DEV_TOOLS OFF)
  else()
    set(SONIC_FULL_DEV_TOOLS ON)
  endif()

  if(SONIC_FULL_DEV_TOOLS)
    # Unit tests: link the exact same compiled game logic (SonicCore) as the
    # real Sonic executable, exercising real production functions directly
    # (see tests/test_plc.c, tests/test_leveldraw.c) rather than
    # reimplementing their logic.
    add_executable(SonicTests "tests/test_main.c" "tests/test_plc.c"
                               "tests/test_leveldraw.c" "tests/test_levelcollision.c")
    target_link_libraries(SonicTests PRIVATE SonicCore)
    set_target_properties(SonicTests PROPERTIES C_STANDARD 99
                                                 C_STANDARD_REQUIRED ON
                                                 C_EXTENSIONS OFF)
    set_target_properties(SonicTests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${BUILD_DIRECTORY}/$<CONFIG>")
    add_test(NAME unit_tests COMMAND SonicTests)
  endif()

  # Headless smoke runner: same entry point as the real game, but with
  # --zone/--act CLI injection to skip straight into a level (see
  # tests/smoke_main.c). Also handy for manual debugging -- no more
  # hand-editing fprintf() calls into GM_Level.c/PLC.c and remembering to
  # revert them afterward.
  add_executable(SonicSmoke "tests/smoke_main.c")
  target_link_libraries(SonicSmoke PRIVATE SonicCore)
  set_target_properties(SonicSmoke PROPERTIES C_STANDARD 99
                                               C_STANDARD_REQUIRED ON
                                               C_EXTENSIONS OFF)

  # Demo recorder: play a level/special stage for real (or via the --ai
  # control hook) and record the input to a demo file in the same format
  # Demo.c plays back -- see tests/demo_record_main.c. Interactive tool, not
  # registered as a ctest.
  add_executable(SonicDemoRecord "tests/demo_record_main.c" "tests/ai_pipe.c"
                                  "tests/timestamp.c")
  target_link_libraries(SonicDemoRecord PRIVATE SonicCore)
  set_target_properties(SonicDemoRecord PROPERTIES C_STANDARD 99
                                                     C_STANDARD_REQUIRED ON
                                                     C_EXTENSIONS OFF)

  # Default --out location: bindir/demos, alongside the built executables,
  # so recordings land somewhere predictable regardless of the working
  # directory SonicDemoRecord is invoked from.
  set(DEMO_OUTPUT_DIR "${BUILD_DIRECTORY}/${CMAKE_BUILD_TYPE}/demos")
  file(MAKE_DIRECTORY "${DEMO_OUTPUT_DIR}")
  target_compile_definitions(SonicDemoRecord PRIVATE DEMO_OUTPUT_DIR="${DEMO_OUTPUT_DIR}")

  if(SONIC_FULL_DEV_TOOLS)
    # Joypad test tool: a small SDL2 window showing a row of icons (the four
    # D-pad directions, then A/B/C/Start) that light up while the matching
    # button is held -- see tests/joypad_test_main.c. Reads input through the
    # same Joypad_GetState1()/Input_HandleEvents() path the real game uses,
    # so it doubles as a manual sanity check for the keyboard scheme and any
    # connected SDL_GameController (including Steam Input). Interactive tool,
    # not registered as a ctest.
    add_executable(SonicJoypadTest "tests/joypad_test_main.c")
    target_link_libraries(SonicJoypadTest PRIVATE SonicCore)
    if(NOT WIN32)
      target_link_libraries(SonicJoypadTest PRIVATE m)
    endif()
    set_target_properties(SonicJoypadTest PROPERTIES C_STANDARD 99
                                                       C_STANDARD_REQUIRED ON
                                                       C_EXTENSIONS OFF)

    # Sound trace tool: loads a real song (by sound ID, default $81 = GHZ) and
    # logs every note/duration event Sound.c's TickChannel decodes to stderr,
    # for direct comparison against the song's own .asm source. Usage:
    # SonicSoundTrace [sound_id_hex] [frames]
    add_executable(SonicSoundTrace "tests/sound_trace_main.c")
    target_link_libraries(SonicSoundTrace PRIVATE SonicCore)

    # Renders a real song/SFX to raw 16-bit stereo PCM for offline waveform
    # inspection, independent of any live audio backend. Usage:
    # SonicSoundWav [sound_id_hex] [seconds] [out.pcm]
    add_executable(SonicSoundWav "tests/sound_wav_main.c")
    target_link_libraries(SonicSoundWav PRIVATE SonicCore)

    # Throwaway smoke test for SMPS driver-version-3 (Sonic 3/Flamedriver-
    # compatible coordination flags) -- see the SMPS driver-version-3 plan's
    # own verification section. Not registered as a ctest.
    add_executable(V3FlagTest "tests/v3_flag_test_main.c")
    target_link_libraries(V3FlagTest PRIVATE SonicCore)

    # A/B verification for the "SMPS runtime: walk JSON directly" plan --
    # renders the same song through the byte-VM and the JSON engine and
    # diffs the PCM. Not registered as a ctest.
    add_executable(JsonABTest "tests/json_ab_test_main.c")
    target_link_libraries(JsonABTest PRIVATE SonicCore)

    # Voice isolation tool: plays a real song at real speed through the actual
    # gameplay driver, muting every FM channel except one currently loaded
    # with a chosen voice index (PSG/DAC muted entirely) -- see
    # tests/voice_isolate_main.c. Live stdin control (type a voice index +
    # Enter to switch), not registered as a ctest. Usage: SonicVoiceIsolate
    # [song_id_hex]
    add_executable(SonicVoiceIsolate "tests/voice_isolate_main.c")
    target_link_libraries(SonicVoiceIsolate PRIVATE SonicCore SDL2::SDL2)
    # Not C_EXTENSIONS OFF here -- needs POSIX select()/fd_set for non-blocking
    # stdin polling, which strict ISO C99 mode doesn't expose without extra
    # feature-test-macro plumbing.

    # SMPSInspector: real-time per-channel activity visualizer (piano-key
    # strips for FM1-6/PSG1-3, squares for the noise slot, hexagons for DAC)
    # inspired by Knuckles Chaotix's extended sound test screen -- see
    # tests/smps_inspector_main.c for the full design note. Left/Right
    # cycles through registered songs while it plays them for real.
    add_executable(SMPSInspector "tests/smps_inspector_main.c")
    target_link_libraries(SMPSInspector PRIVATE SonicCore SDL2::SDL2)
    set_target_properties(SMPSInspector PROPERTIES C_STANDARD 99
                                                     C_STANDARD_REQUIRED ON
                                                     C_EXTENSIONS OFF)

    set(IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/contrib/imgui")
    set(NUKED_OPN2_DIR "${CMAKE_CURRENT_SOURCE_DIR}/contrib/nuked-opn2")

    # ParadoxComposer: Qt6 editor/DAW for the ParadoxSMPS JSON song/SFX
    # format (tools/paradoxsmps/), replacing SonicVoiceEditor entirely --
    # see tools/paradoxcomposer/ for the individual panel source files.
    # Compiles in-process by linking libparadoxsmps directly (NativeCompiler.cpp
    # is just the QJsonObject/QByteArray boundary wrapper around it -- the
    # SAME static library jsonc2h_tool and the real CMake build use, so there
    # is only one compiler implementation anywhere in this project), and
    # plays back either a single voice directly (Backend/YM2612.h, same as
    # the old voice editor) or a full compiled song through SonicCore's real
    # sequencer (Sound_DebugPlayRawSong, Sound.c).
    find_package(Qt6 REQUIRED COMPONENTS Widgets)
    add_executable(ParadoxComposer
      "tools/paradoxcomposer/main.cpp"
      "tools/paradoxcomposer/MainWindow.cpp"
      "tools/paradoxcomposer/MainWindow.h"
      "tools/paradoxcomposer/SongDocument.cpp"
      "tools/paradoxcomposer/SongDocument.h"
      "tools/paradoxcomposer/NativeCompiler.cpp"
      "tools/paradoxcomposer/NativeCompiler.h"
      "tools/paradoxcomposer/AudioEngine.cpp"
      "tools/paradoxcomposer/AudioEngine.h"
      "tools/paradoxcomposer/SourcePanel.cpp"
      "tools/paradoxcomposer/SourcePanel.h"
      "tools/paradoxcomposer/BytesPanel.cpp"
      "tools/paradoxcomposer/BytesPanel.h"
      "tools/paradoxcomposer/VoiceBankPanel.cpp"
      "tools/paradoxcomposer/VoiceBankPanel.h"
      "tools/paradoxcomposer/VoiceWidgets.cpp"
      "tools/paradoxcomposer/VoiceWidgets.h"
      "tools/paradoxcomposer/PlaylistPanel.cpp"
      "tools/paradoxcomposer/PlaylistPanel.h"
      "tools/paradoxcomposer/PianoRollWidgets.cpp"
      "tools/paradoxcomposer/PianoRollWidgets.h"
      "tools/paradoxcomposer/PianoRollPanel.cpp"
      "tools/paradoxcomposer/PianoRollPanel.h"
      "tools/paradoxcomposer/WaveformVUWidget.cpp"
      "tools/paradoxcomposer/WaveformVUWidget.h"
      "tools/paradoxcomposer/ChannelsPanel.cpp"
      "tools/paradoxcomposer/ChannelsPanel.h"
      "tools/paradoxcomposer/PlaybackPanel.cpp"
      "tools/paradoxcomposer/PlaybackPanel.h"
      "tools/paradoxcomposer/DockableContainer.cpp"
      "tools/paradoxcomposer/DockableContainer.h"
      "tools/paradoxcomposer/BlockMetadata.cpp"
      "tools/paradoxcomposer/BlockMetadata.h")
    target_link_libraries(ParadoxComposer PRIVATE SonicCore paradoxsmps SDL2::SDL2 Qt6::Widgets)
    target_compile_definitions(ParadoxComposer PRIVATE
      PARADOXCOMPOSER_REPO_ROOT="${CMAKE_SOURCE_DIR}")
    set_target_properties(ParadoxComposer PROPERTIES
      CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON
      AUTOMOC ON)

    # Throwaway correctness check, not part of the shipped GUI: compiles a
    # .jsonc to raw bytes via NativeCompiler (-> libparadoxsmps) and writes
    # them to stdout, so they can be byte-diffed against jsonc2h_tool's own
    # output on the same file -- see native_compiler_test_main.cpp.
    add_executable(NativeCompilerTest
      "tools/paradoxcomposer/native_compiler_test_main.cpp"
      "tools/paradoxcomposer/SongDocument.cpp"
      "tools/paradoxcomposer/SongDocument.h"
      "tools/paradoxcomposer/NativeCompiler.cpp"
      "tools/paradoxcomposer/NativeCompiler.h")
    target_link_libraries(NativeCompilerTest PRIVATE Qt6::Core paradoxsmps)
    set_target_properties(NativeCompilerTest PROPERTIES
      CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON
      AUTOMOC ON)

    # fmcore's interactive test rig -- home-row-piano input driving a full
    # 4-operator FMVoice (fm_voice.h) through a free-form, Flowstone-style
    # patch-cable UI (see fmcore/fmcore_synth_main.cpp's own design note).
    # Evolved from an earlier single-operator-only version (fmcore_gui_main.cpp,
    # now removed/superseded). Vendors Dear ImGui (imgui/ submodule) directly,
    # independent of ParadoxComposer's Qt6 UI, but otherwise
    # has nothing to do with SonicCore/the game -- fmcore is a from-scratch
    # YM2612 side project, not wired into gameplay yet.
    add_executable(FMCoreSynth
      "fmcore/fm_operator.c"
      "fmcore/fm_operator.h"
      "fmcore/fm_voice.c"
      "fmcore/fm_voice.h"
      "fmcore/fmcore_synth_main.cpp"
      "${IMGUI_DIR}/imgui.cpp"
      "${IMGUI_DIR}/imgui_draw.cpp"
      "${IMGUI_DIR}/imgui_tables.cpp"
      "${IMGUI_DIR}/imgui_widgets.cpp"
      "${IMGUI_DIR}/imgui_demo.cpp"
      "${IMGUI_DIR}/backends/imgui_impl_sdl2.cpp"
      "${IMGUI_DIR}/backends/imgui_impl_sdlrenderer2.cpp")
    target_include_directories(FMCoreSynth PRIVATE "fmcore" "${IMGUI_DIR}" "${IMGUI_DIR}/backends")
    target_link_libraries(FMCoreSynth PRIVATE SDL2::SDL2)
    set_target_properties(FMCoreSynth PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
    set_source_files_properties("fmcore/fm_operator.c" "fmcore/fm_voice.c" PROPERTIES LANGUAGE C)

    set_target_properties(
      SonicJoypadTest SonicSoundTrace SonicSoundWav V3FlagTest ParadoxComposer SonicVoiceIsolate SMPSInspector FMCoreSynth
      PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${BUILD_DIRECTORY}/$<CONFIG>")
  endif()

  set_target_properties(SonicSmoke SonicDemoRecord PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${BUILD_DIRECTORY}/$<CONFIG>")

  # One headless smoke test per zone. "Still running when the timeout hits"
  # is success (the game loop never exits on its own) -- a crash, an early
  # exit, or an ASan abort (build with -DSANITIZE=ON for that coverage) is a
  # failure. See tests/RunSmoke.cmake.
  set(SMOKE_TARGETS "0:GHZ" "1:LZ" "2:MZ" "3:SLZ" "4:SYZ" "5:SBZ")
  foreach(ENTRY IN LISTS SMOKE_TARGETS)
    string(REPLACE ":" ";" PAIR "${ENTRY}")
    list(GET PAIR 0 ZONE_NUM)
    list(GET PAIR 1 ZONE_NAME)
    add_test(
      NAME smoke_${ZONE_NAME}
      COMMAND
        ${CMAKE_COMMAND} -DTEST_EXE=$<TARGET_FILE:SonicSmoke>
        "-DTEST_ARGS=--zone;${ZONE_NUM}" -DTEST_TIMEOUT=15 -P
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/RunSmoke.cmake")
    set_tests_properties(smoke_${ZONE_NAME} PROPERTIES TIMEOUT 30)
  endforeach()
endif()
