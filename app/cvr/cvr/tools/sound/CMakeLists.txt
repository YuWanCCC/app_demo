cmake_minimum_required(VERSION 3.1 FATAL_ERROR)

project(sound_tool)

#--------------------------
# cvr sound tool
#--------------------------
if(SOC_RV1126)
install(PROGRAMS rv1126/asound.state DESTINATION "bin")
install(PROGRAMS rv1126/alsactl DESTINATION "bin")
install(PROGRAMS rv1126/amixer DESTINATION "bin")
install(FILES rv1126/libasound.so.2 DESTINATION "lib")
install(FILES rv1126/asound.conf DESTINATION "root/etc")
install(FILES rv1126/asound.sh DESTINATION "bin")
endif()

if(SOC_RV1126B)
install(FILES rv1126b/asound.sh DESTINATION "bin")
endif()