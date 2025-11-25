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
# TODO ! PACKAGE_DEBUG_INFO needed? Try causing error in zplayer CI job and see if stack trace is given.
if(APPLE)
    set(CPACK_GENERATOR "DragNDrop")
    set(CPACK_PACKAGING_INSTALL_PREFIX "/")
    set(CPACK_DMG_VOLUME_NAME "ZQuest Classic")
	set(CPACK_DMG_DS_STORE_SETUP_SCRIPT ${PROJECT_SOURCE_DIR}/packaging/MacDMGSetup.scpt)
	set(CPACK_DMG_BACKGROUND_IMAGE ${PROJECT_SOURCE_DIR}/packaging/MacDMGBackground.png)

	install(FILES "${PROJECT_SOURCE_DIR}/scripts/Info.plist" DESTINATION "ZQuest Classic.app/Contents")
	install(PROGRAMS "${PROJECT_SOURCE_DIR}/packaging/mac_entry.sh" DESTINATION "ZQuest Classic.app/Contents/MacOS")

	# Icons.
	set(ICON_SCRIPT "${PROJECT_SOURCE_DIR}/packaging/generate_mac_bundle_icons.sh")
	install(CODE "
		execute_process(
			COMMAND bash \"${ICON_SCRIPT}\" \"ZQuest Classic.app/Contents/Resources/icons.icns\"
			COMMAND_ERROR_IS_FATAL ANY
		)
	")

	# Generate debug info bundle (ZQuest Classic.app.dSYM) and strip debug info from
	# .app.
	# install(CODE "
	# 	if(DEFINED ENV{DESTDIR})
	# 		set(INSTALL_ROOT \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}\")
	# 	else()
	# 		set(INSTALL_ROOT \"\${CMAKE_INSTALL_PREFIX}\")
	# 	endif()

	# 	set(APP_BUNDLE \"\${INSTALL_ROOT}/ZQuest Classic.app\")
	# 	set(DSYM_BUNDLE \"\${INSTALL_ROOT}/ZQuest Classic.app.dSYM\")

	# 	file(GLOB_RECURSE CANDIDATES \"\${APP_BUNDLE}/Contents/Resources/*\")
	# 	set(EXECUTABLES \"\")
	# 	foreach(ITEM \${CANDIDATES})
	# 		# Skip directories
	# 		if(IS_DIRECTORY \"\${ITEM}\")
	# 			continue()
	# 		endif()

	# 		execute_process(
	# 			COMMAND file -b \"\${ITEM}\"
	# 			OUTPUT_VARIABLE FILE_TYPE
	# 			OUTPUT_STRIP_TRAILING_WHITESPACE
	# 		)

	# 		if(FILE_TYPE MATCHES \"Mach-O.*executable\")
	# 			message(STATUS \"Found executable: \${ITEM}\")
	# 			list(APPEND EXECUTABLES \"\${ITEM}\")
	# 		endif()
	# 	endforeach()

	# 	file(GLOB_RECURSE DYLIBS \"\${APP_BUNDLE}/Contents/libs/*.dylib\")

	# 	set(ALL_BINARIES \${EXECUTABLES} \${DYLIBS})

	# 	message(STATUS \"Generating debug symbols: \${DSYM_BUNDLE}\")
	# 	execute_process(
	# 		COMMAND xcrun dsymutil 
	# 		-o \"\${DSYM_BUNDLE}\" 
	# 		\${ALL_BINARIES}
	# 		RESULT_VARIABLE DSYM_RESULT
	# 		OUTPUT_VARIABLE DSYM_OUT
	# 		ERROR_VARIABLE DSYM_ERR
	# 	)

	# 	if(NOT DSYM_RESULT EQUAL 0)
	# 		message(FATAL_ERROR \"Failed to generate dSYM: \${DSYM_ERR}\")
	# 	endif()

	# 	message(STATUS \"Stripping debug info from binaries...\")
	# 	foreach(BIN \${ALL_BINARIES})
	# 		execute_process(
	# 			COMMAND xcrun strip -S \"\${BIN}\"
	# 			RESULT_VARIABLE STRIP_RESULT
	# 			ERROR_VARIABLE STRIP_ERR
	# 		)
	# 		if(NOT STRIP_RESULT EQUAL 0)
	# 			message(WARNING \"Could not strip \${BIN}: \${STRIP_ERR}\")
	# 		endif()
	# 	endforeach()
	# ")

	# Fixes the binaries to make them portable.
	install(CODE "
		include(BundleUtilities)
		fixup_bundle(\"$<INSTALL_PREFIX>/ZQuest Classic.app\" \"\" \"$<INSTALL_PREFIX>/ZQuest Classic.app/Contents/Frameworks\")
	")
elseif(LINUX)
	set(CPACK_GENERATOR "TGZ")
elseif(WIN32)
	set(CPACK_GENERATOR "ZIP")
endif()

include(CPack)
