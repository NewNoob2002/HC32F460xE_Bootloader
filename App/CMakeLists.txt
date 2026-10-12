function(add_app_product product)
    include(${CMAKE_CURRENT_SOURCE_DIR}/Products/${product}/product.cmake)
    if(APP_USE_LVGL AND NOT APP_HAS_OLED)
        message(FATAL_ERROR "${product}: LVGL requires OLED in these products")
    endif()
    add_library(app_${product} INTERFACE)
    add_library(app::${product} ALIAS app_${product})
    target_compile_definitions(app_${product} INTERFACE
        APP_PRODUCT_NAME="${product}"
        APP_HAS_OLED=$<BOOL:${APP_HAS_OLED}>
        APP_USE_LVGL=$<BOOL:${APP_USE_LVGL}>
    )
    target_link_libraries(app_${product} INTERFACE firmware::config)
    set_property(TARGET app_${product} PROPERTY INTERFACE_HC32_APP_PRODUCT ${product})
    set_property(TARGET app_${product} APPEND PROPERTY COMPATIBLE_INTERFACE_STRING HC32_APP_PRODUCT)
endfunction()

# 产品配置目标；实际 App 入口及业务源码将在接入时加入对应目录。
add_app_product(X1)
add_app_product(X1Pro)
add_app_product(XBase)

if(NOT BOOT_BUILD_HOST_TESTS)
    # 镜像链接配置与产品配置分开，主机测试不继承 ARM 链接选项。
    add_library(hc32_app_image INTERFACE)
    add_library(app::image ALIAS hc32_app_image)
    set(APP_LINKER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/Linker/HC32F460_app.ld)
    set(HC32_LINKER_DIR ${PROJECT_SOURCE_DIR}/Platform/HC32F460/Linker)
    target_link_libraries(hc32_app_image INTERFACE hc32::platform)
    target_compile_definitions(hc32_app_image INTERFACE VECT_TAB_OFFSET=0x00008000U)
    target_link_options(hc32_app_image INTERFACE
        ${CPU_FLAGS} -Wl,-L,${HC32_LINKER_DIR} -T${APP_LINKER_SCRIPT}
        -Wl,--gc-sections --specs=nano.specs --specs=nosys.specs
    )
    set_property(TARGET hc32_app_image PROPERTY INTERFACE_LINK_DEPENDS
        "${APP_LINKER_SCRIPT};${HC32_LINKER_DIR}/HC32F460_sections.ld")
endif()
