idf_component_register(
        SRCS "main.cpp" "network.cpp" "runtime.cpp" "gyro.cpp" "sample.cpp" "lsm6dsm_reg.c" "persistent.cpp" "dac.cpp" "server.cpp" "buffer.cpp" "socket.cpp" "indicator.cpp" "controller.cpp" "settings.cpp"
        INCLUDE_DIRS "."
        EMBED_FILES "style.css")