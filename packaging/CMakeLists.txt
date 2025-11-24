# add_custom_command(
# 	COMMAND ${PYTHON} ${PROJECT_SOURCE_DIR}/scripts/package.py --skip_binaries --copy_to_build_folder --build_folder "${BUILD_FOLDER}" --keep_existing_files
# 	OUTPUT copy_resources_if_not_exist.stamp
# )
# add_custom_target(copy_resources_if_not_exist DEPENDS copy_resources_if_not_exist.stamp)
# set_source_files_properties(copy_resources_if_not_exist.stamp PROPERTIES SYMBOLIC 1)

# add_custom_command(
# 	COMMAND ${PYTHON} ${PROJECT_SOURCE_DIR}/scripts/package.py --skip_binaries --copy_to_build_folder --build_folder "${BUILD_FOLDER}"
# 	OUTPUT copy_resources.stamp
# )
# add_custom_target(copy_resources DEPENDS copy_resources.stamp)
# set_source_files_properties(copy_resources.stamp PROPERTIES SYMBOLIC 1)

# add_custom_command(
# 	COMMAND ${PYTHON} ${PROJECT_SOURCE_DIR}/scripts/package.py --build_folder "${BUILD_FOLDER}" --clean_first
# 	OUTPUT package.stamp
# 	DEPENDS ${zc_targets}
# )
# add_custom_target(package_zc DEPENDS package.stamp)
# set_source_files_properties(package.stamp PROPERTIES SYMBOLIC 1)

# add_custom_command(
# 	COMMAND ${PYTHON} ${PROJECT_SOURCE_DIR}/scripts/package.py --build_folder "${BUILD_FOLDER}" --clean_first --skip_archive
# 	OUTPUT package_no_archive.stamp
# 	DEPENDS ${zc_targets}
# )
# add_custom_target(package_zc_no_archive DEPENDS package_no_archive.stamp)
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

# install(CODE "
# 	execute_process(
# 		COMMAND ${PYTHON} \"${PROJECT_SOURCE_DIR}/scripts/package.py\"
# 		--skip_binaries
# 		--skip_archive
# 		$<$<BOOL:${ZC_VERSION}>:--version;${ZC_VERSION}>
# 		--build_folder \"\${BUILD_FOLDER}\"
# 		--package_folder \"$<INSTALL_PREFIX>/${INSTALL_DATA_DIR}\"
# 		--keep_existing_files
# 		COMMAND_ERROR_IS_FATAL ANY)
# ")

install(DIRECTORY "${PROJECT_SOURCE_DIR}/resources/" DESTINATION "${INSTALL_DATA_DIR}")
install(DIRECTORY "${PROJECT_SOURCE_DIR}/changelogs" DESTINATION "${INSTALL_DATA_DIR}")

# TODO !
set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY OFF)

# TODO ! replace above with an install(FILES )?
# TODO ! generate_licenses at install time ... above command should handle but double check it. and maybe do as a seperate install-time script.
# TODO ! zstandalone ? crashpad handler ? zconsole ?
# TODO ! test_runner ? add WANT_TESTS
# TODO ! run generate_mac_bundle_icons at install time?
# TODO ! PACKAGE_DEBUG_INFO needed? Try causing error in zplayer CI job and see if stack trace is given.
if(APPLE)
    set(CPACK_GENERATOR "DragNDrop")
    set(CPACK_PACKAGING_INSTALL_PREFIX "/")
    set(CPACK_DMG_VOLUME_NAME "ZQuest Classic")
	set(CPACK_DMG_DS_STORE_SETUP_SCRIPT ${PROJECT_SOURCE_DIR}/packaging/MacDMGSetup.scpt)
	set(CPACK_DMG_BACKGROUND_IMAGE ${PROJECT_SOURCE_DIR}/packaging/MacDMGBackground.png)

	install(FILES "${PROJECT_SOURCE_DIR}/scripts/Info.plist" DESTINATION "ZQuest Classic.app/Contents")
	install(FILES "${PROJECT_SOURCE_DIR}/packaging/icons.icns" DESTINATION "ZQuest Classic.app/Contents/Resources")
	# install(
	# 	FILES ${PROJECT_SOURCE_DIR}/packaging/mac_entry.sh
	# 	PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ
	# 	DESTINATION "ZQuest Classic.app/Contents/MacOS"
	# )
	install(PROGRAMS "${PROJECT_SOURCE_DIR}/packaging/mac_entry.sh" DESTINATION "ZQuest Classic.app/Contents/MacOS")

	# Fixes the binaries to make them portable.
	install(CODE "
		include(BundleUtilities)
		fixup_bundle(\"$<INSTALL_PREFIX>/ZQuest Classic.app\" \"\" \"$<INSTALL_PREFIX>/ZQuest Classic.app/Contents/Frameworks\")
	")
	# TODO ! verify_app?
elseif(LINUX)
	set(CPACK_GENERATOR "TGZ")
elseif(WIN32)
	set(CPACK_GENERATOR "ZIP")
endif()

include(CPack)
