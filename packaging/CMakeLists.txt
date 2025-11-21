# add_custom_command(
# 	COMMAND ${PYTHON} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/package.py --skip_binaries --copy_to_build_folder --build_folder "${BUILD_FOLDER}" --keep_existing_files
# 	OUTPUT copy_resources_if_not_exist.stamp
# )
# add_custom_target(copy_resources_if_not_exist DEPENDS copy_resources_if_not_exist.stamp)
# set_source_files_properties(copy_resources_if_not_exist.stamp PROPERTIES SYMBOLIC 1)

# add_custom_command(
# 	COMMAND ${PYTHON} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/package.py --skip_binaries --copy_to_build_folder --build_folder "${BUILD_FOLDER}"
# 	OUTPUT copy_resources.stamp
# )
# add_custom_target(copy_resources DEPENDS copy_resources.stamp)
# set_source_files_properties(copy_resources.stamp PROPERTIES SYMBOLIC 1)

# add_custom_command(
# 	COMMAND ${PYTHON} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/package.py --build_folder "${BUILD_FOLDER}" --clean_first
# 	OUTPUT package.stamp
# 	DEPENDS ${zc_targets}
# )
# add_custom_target(package DEPENDS package.stamp)
# set_source_files_properties(package.stamp PROPERTIES SYMBOLIC 1)

# add_custom_command(
# 	COMMAND ${PYTHON} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/package.py --build_folder "${BUILD_FOLDER}" --clean_first --skip_archive
# 	OUTPUT package_no_archive.stamp
# 	DEPENDS ${zc_targets}
# )
# add_custom_target(package_no_archive DEPENDS package_no_archive.stamp)
# set_source_files_properties(package_no_archive.stamp PROPERTIES SYMBOLIC 1)

# if(COPY_RESOURCES AND NOT EMSCRIPTEN)
# 	foreach(target IN LISTS zc_targets)
# 		add_dependencies(${target} copy_resources_if_not_exist)
# 	endforeach()
# endif()

if(APPLE)
    set(INSTALL_DATA_DIR "ZQuest Classic.app/Contents/Resources")
elseif(WIN32)
    set(INSTALL_DATA_DIR ".")
else()
    set(INSTALL_DATA_DIR "share/zquestclassic")
endif()

install(CODE "
	execute_process(
		COMMAND ${PYTHON} \"${PROJECT_SOURCE_DIR}/scripts/package.py\"
		--skip_binaries
		--skip_archive
		--build_folder \"\${BUILD_FOLDER}\"
		--package_folder \"$<INSTALL_PREFIX>/${INSTALL_DATA_DIR}\"
		--keep_existing_files
		COMMAND_ERROR_IS_FATAL ANY)
")

if(APPLE)
	# file(WRITE "${CMAKE_BINARY_DIR}/dummy_main.cpp" "int main() { return 0; }")

	# add_executable(ZQuestClassicAppBundle "${CMAKE_BINARY_DIR}/dummy_main.cpp")
	# add_dependencies(ZQuestClassicAppBundle zlauncher zplayer zeditor zscript)

	# set_target_properties(ZQuestClassicAppBundle PROPERTIES
	# 	MACOSX_BUNDLE TRUE
	# 	OUTPUT_NAME "ZQuest Classic"
	# 	MACOSX_BUNDLE_BUNDLE_NAME "ZQuest Classic"
	# 	MACOSX_BUNDLE_GUI_IDENTIFIER "com.zquestclassic.www"
	# 	MACOSX_BUNDLE_INFO_PLIST_KEYS "
	# 		<key>CFBundleExecutable</key>
	# 		<string>zlauncher</string>
	# 	"
	# )

	# zc_install_targets(ZQuestClassicAppBundle)

    set(CPACK_GENERATOR "DragNDrop")
    set(CPACK_PACKAGING_INSTALL_PREFIX "/")
	# set(CPACK_SET_DESTDIR true)
	# set(CPACK_BUNDLE_NAME "ZQuest Classic")
    set(CPACK_DMG_VOLUME_NAME "ZQuest Classic")
	# set(CPACK_BUNDLE_PLIST ${PROJECT_SOURCE_DIR}/scripts/Info.plist)
	# set(CPACK_BUNDLE_ICON ${PROJECT_SOURCE_DIR}/packaging/icons.icns)
	# CPACK_BUNDLE_STARTUP_COMMAND

	install(FILES ${PROJECT_SOURCE_DIR}/scripts/Info.plist DESTINATION "ZQuest Classic.app/Contents")
	install(
		FILES ${PROJECT_SOURCE_DIR}/scripts/mac_entry.sh
		PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ
		DESTINATION "ZQuest Classic.app/Contents/MacOS"
	)

	install(CODE "
			include(BundleUtilities)
			fixup_bundle(\"$<INSTALL_PREFIX>/ZQuest Classic.app\" \"\" \"$<INSTALL_PREFIX>/ZQuest Classic.app/Contents/Frameworks\")")

    # set(MACOSX_BUNDLE_BUNDLE_NAME ${CPACK_PACKAGE_NAME})
    # set(MACOSX_BUNDLE_BUNDLE_GUI_IDENTIFIER "com.zquestclassic.www")
    # set(MACOSX_BUNDLE_ICON_FILE ${PROJECT_SOURCE_DIR}/packaging/ exampleApp.icns)
    # set(MACOSX_BUNDLE_INFO_PLIST ${PROJECT_SOURCE_DIR}/scripts/Info.plist)
    # set(MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}")
    # set_source_files_properties(${PROJECT_SOURCE_DIR}/packaging/icons.icns PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")

    # set(CPACK_DMG_DS_STORE_SETUP_SCRIPT "${PROJECT_SOURCE_DIR}/packaging/DS_Store.scpt") 
    # set(CPACK_DMG_BACKGROUND_IMAGE "${PROJECT_SOURCE_DIR}/packaging/dmg_background.png")
    # set(CPACK_OSX_PACKAGE_VERSION "10.6") #min package version

	# TODO ! fixup bundle?
endif()

include(CPack)
