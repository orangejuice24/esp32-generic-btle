set(COMPONENT_SRCS
    "generic_btle.c"
    "main.c"
    "gatt_svr.c"
    "gatt_vars.c"
    "ble_func.c"
)

set(COMPONENT_ADD_INCLUDEDIRS
    "."
)

register_component()
