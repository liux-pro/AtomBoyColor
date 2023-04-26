idf_component_register(SRC_DIRS "."
        INCLUDE_DIRS ".")
#警告不被视为错误
target_compile_options(${COMPONENT_LIB} PRIVATE -w)
#关闭音频
target_compile_options(${COMPONENT_LIB} PRIVATE -DENABLE_NES_AUDIO=1)