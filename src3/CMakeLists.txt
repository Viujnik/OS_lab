cmake_minimum_required(VERSION 3.16)
project(os_lab5 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Один исходник компилируется дважды. Макрос выбирает ветку #if в C.
# SHARED даёт отдельный файл .so на Linux и .dylib на macOS.
foreach(number RANGE 1 2)
    add_library(lab5_impl${number} SHARED implementation.c)
    target_compile_definitions(lab5_impl${number} PRIVATE IMPLEMENTATION=${number})
    target_link_libraries(lab5_impl${number} PRIVATE m)
endforeach()

# linked.c даёт main программы №1, cli.c — общие чтение и печать.
# Ссылка на lab5_impl1 фиксирует обязательную зависимость при компоновке.
add_executable(lab5_linked linked.c cli.c)
target_link_libraries(lab5_linked PRIVATE lab5_impl1)
set_target_properties(lab5_linked PROPERTIES BUILD_RPATH_USE_ORIGIN ON)

# runtime.c даёт main программы №2. CMAKE_DL_LIBS обозначает системную
# поддержку dlopen. На платформах без отдельной libdl значение пустое.
add_executable(lab5_runtime runtime.c cli.c)
target_link_libraries(lab5_runtime PRIVATE ${CMAKE_DL_LIBS})
# Собираем файлы для dlopen, сохраняя выбор библиотеки за самой программой.
add_dependencies(lab5_runtime lab5_impl1 lab5_impl2)
