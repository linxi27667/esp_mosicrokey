idf_component_register(SRCS "espclaw_adapter.cc"
    EMBED_TXTFILES "../../system/skills/game-creator/SKILL.md" "../../examples/snake/main.lua"
    INCLUDE_DIRS "include" REQUIRES mosicrokey_contracts
    PRIV_REQUIRES claw_core claw_cap claw_memory claw_skill claw_event_router claw_manager nvs_flash board_support espressif__cjson)
