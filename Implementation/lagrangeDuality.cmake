set(LD_NAME lagrangeDuality)

file(GLOB LD_SOURCES     ${CMAKE_CURRENT_LIST_DIR}/src/*.cpp)
file(GLOB LD_INCS        ${CMAKE_CURRENT_LIST_DIR}/src/*.h)
file(GLOB LD_INCS_CORE   ${CMAKE_CURRENT_LIST_DIR}/src/core/*.h)
file(GLOB LD_INCS_VIEWS  ${CMAKE_CURRENT_LIST_DIR}/src/views/*.h)
set(LD_PLIST             ${CMAKE_CURRENT_LIST_DIR}/src/Info.plist)
file(GLOB LD_INC_TD      ${NATID_SDK_INC}/td/*.h)
file(GLOB LD_INC_GUI     ${NATID_SDK_INC}/gui/*.h)

add_executable(${LD_NAME} ${LD_SOURCES} ${LD_INCS} ${LD_INCS_CORE} ${LD_INCS_VIEWS}
                          ${LD_INC_TD} ${LD_INC_GUI})

target_include_directories(${LD_NAME} PRIVATE ${CMAKE_CURRENT_LIST_DIR}/src)

source_group("inc"          FILES ${LD_INCS})
source_group("inc\\core"    FILES ${LD_INCS_CORE})
source_group("inc\\views"   FILES ${LD_INCS_VIEWS})
source_group("inc\\td"      FILES ${LD_INC_TD})
source_group("inc\\gui"     FILES ${LD_INC_GUI})
source_group("src"          FILES ${LD_SOURCES})

target_link_libraries(${LD_NAME} debug ${MU_LIB_DEBUG} debug ${NATGUI_LIB_DEBUG}
                                 optimized ${MU_LIB_RELEASE} optimized ${NATGUI_LIB_RELEASE})

setTargetPropertiesForGUIApp(${LD_NAME} ${LD_PLIST})
setIDEPropertiesForGUIExecutable(${LD_NAME} ${CMAKE_CURRENT_LIST_DIR})
setPlatformDLLPath(${LD_NAME})
