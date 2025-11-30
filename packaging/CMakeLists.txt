set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY OFF)

# TODO !
if(NOT EMSCRIPTEN AND WIN32)
	# TODO !
	zc_install_targets(allegro zcsound)

	# TODO ! all at once?
	set(MY_GAME_TARGETS zlauncher zplayer zeditor zscript zupdater allegro_acodec)

	foreach(app ${MY_GAME_TARGETS})
		set(INSTALL_BIN_DIR "bin")
		set(INSTALL_LIB_DIR "lib")
		if(APPLE)
			set(INSTALL_BIN_DIR "ZQuest Classic.app/Contents/MacOS")
			set(INSTALL_LIB_DIR "ZQuest Classic.app/Contents/Frameworks")
		elseif(WIN32)
			set(INSTALL_BIN_DIR ".")
			set(INSTALL_LIB_DIR ".")
		elseif(LINUX)
			# TODO ! remove this branch.
			set(INSTALL_BIN_DIR ".")
			set(INSTALL_LIB_DIR ".")
		endif()

		get_all_linked_targets("${app}" ALL_DEPS)

		# For debugging.
		# message(STATUS "---------------------------------------------------")
		# message(STATUS "DEBUG: Analyzing dependencies for app: ${app}")
		# foreach(dep ${ALL_DEPS})
		# 	get_target_property(type ${dep} TYPE)
		# 	# Attempt to get the directory, but don't crash if it fails
		# 	set(dir "NOT_FOUND")
		# 	if(TARGET ${dep})
		# 		# Check if we can get the file location
		# 		get_property(is_imported TARGET ${dep} PROPERTY IMPORTED)
		# 		if(is_imported OR type STREQUAL "SHARED_LIBRARY" OR type STREQUAL "STATIC_LIBRARY")
		# 				# This gen-ex prints the path at configure time for us to see
		# 				set(dir "$<TARGET_FILE_DIR:${dep}>") 
		# 		endif()
		# 	endif()
		# 	message(STATUS "   Dependency: ${dep} [Type: ${type}] -> Dir: ${dir}")
		# endforeach()
		# message(STATUS "---------------------------------------------------")

		set(SEARCH_DIRS "")
		foreach(dep ${ALL_DEPS})
			# Only look at targets that are actual libraries/executables (skip Headers/Interface)
			get_target_property(type ${dep} TYPE)
			if(NOT type STREQUAL "INTERFACE_LIBRARY" AND NOT type STREQUAL "UTILITY")
				list(APPEND SEARCH_DIRS "$<TARGET_FILE_DIR:${dep}>")
			endif()
		endforeach()

		install(TARGETS ${app} 
			RUNTIME_DEPENDENCIES
				DIRECTORIES "${SEARCH_DIRS}"
				PRE_EXCLUDE_REGEXES "api-ms-" "ext-ms-" 
				POST_EXCLUDE_REGEXES ".*system32/.*\\.dll"

			RUNTIME DESTINATION "${INSTALL_BIN_DIR}"
			LIBRARY DESTINATION "${INSTALL_LIB_DIR}"
			FRAMEWORK DESTINATION "${INSTALL_LIB_DIR}"
			BUNDLE DESTINATION .
		)
	endforeach()
endif()

if(APPLE)
	set(INSTALL_DATA_DIR "ZQuest Classic.app/Contents/Resources")
elseif(WIN32)
	set(INSTALL_DATA_DIR ".")
else()
	# TODO !
	# set(INSTALL_DATA_DIR "share/zquestclassic")
	set(INSTALL_DATA_DIR ".")
endif()

install(DIRECTORY "${CMAKE_SOURCE_DIR}/resources/" DESTINATION "${INSTALL_DATA_DIR}")
install(DIRECTORY "${CMAKE_SOURCE_DIR}/changelogs" DESTINATION "${INSTALL_DATA_DIR}")

install(CODE "
	execute_process(
		COMMAND ${PYTHON} \"${CMAKE_SOURCE_DIR}/scripts/package.py\"
		--new_packager
		--build_folder \"\${BUILD_FOLDER}\"
		--package_folder \"$<INSTALL_PREFIX>/${INSTALL_DATA_DIR}\"
		$<$<BOOL:${ZC_VERSION}>:--version;${ZC_VERSION}>
		COMMAND_ERROR_IS_FATAL ANY)
")

# TODO ! rm
# if(WIN32)
# 	get_target_property(CURL_LOCATION CURL::libcurl LOCATION)
# 	install(FILES "${CURL_LOCATION}"
# 			DESTINATION "."
# 			CONFIGURATIONS Release RelWithDebInfo
# 	)
# endif()

# TODO ! zstandalone ? crashpad handler ? zconsole ?
# TODO ! test_runner ? add WANT_TESTS
# TODO ! PACKAGE_DEBUG_INFO needed? Try causing error in zplayer CI job and see if stack trace is given.
if(APPLE)
    set(CPACK_GENERATOR "DragNDrop")
    set(CPACK_PACKAGING_INSTALL_PREFIX "/")
    set(CPACK_DMG_VOLUME_NAME "ZQuest Classic")
	set(CPACK_DMG_DS_STORE_SETUP_SCRIPT "${CMAKE_SOURCE_DIR}/packaging/MacDMGSetup.scpt")
	set(CPACK_DMG_BACKGROUND_IMAGE "${CMAKE_SOURCE_DIR}/packaging/MacDMGBackground.png")

	install(FILES "${CMAKE_SOURCE_DIR}/scripts/Info.plist" DESTINATION "ZQuest Classic.app/Contents")
	install(PROGRAMS "${CMAKE_SOURCE_DIR}/packaging/mac_entry.sh" DESTINATION "ZQuest Classic.app/Contents/MacOS")

	# Icons.
	set(ICON_SCRIPT "${CMAKE_SOURCE_DIR}/packaging/generate_mac_bundle_icons.sh")
	install(CODE "
		execute_process(
			COMMAND bash \"${ICON_SCRIPT}\" \"$<INSTALL_PREFIX>/ZQuest Classic.app/Contents/Resources/icons.icns\"
			COMMAND_ERROR_IS_FATAL ANY
		)
	")

	# Generate debug info bundle (ZQuest Classic.app.dSYM):
	#   cpack -D CPACK_CUSTOM_INSTALL_VARIABLES=MAC_CREATE_DEBUG_SYMBOLS_BUNDLE=TRUE
	# This is only used for testing in CI. For releases, the debug symbols are stripped and sent to Sentry directly.
	install(CODE "
		if(MAC_CREATE_DEBUG_SYMBOLS_BUNDLE)
			set(APP_BUNDLE \"$<INSTALL_PREFIX>/ZQuest Classic.app\")
			set(DSYM_BUNDLE \"$<INSTALL_PREFIX>/ZQuest Classic.app.dSYM\")

			file(GLOB_RECURSE CANDIDATES \"\${APP_BUNDLE}/Contents/Resources/*\")
			set(EXECUTABLES \"\")
			foreach(ITEM \${CANDIDATES})
				if(IS_DIRECTORY \"\${ITEM}\")
					continue()
				endif()

				execute_process(
					COMMAND file -b \"\${ITEM}\"
					OUTPUT_VARIABLE FILE_TYPE
					OUTPUT_STRIP_TRAILING_WHITESPACE
				)

				if(FILE_TYPE MATCHES \"Mach-O.*executable\")
					message(STATUS \"Found executable: \${ITEM}\")
					list(APPEND EXECUTABLES \"\${ITEM}\")
				endif()
			endforeach()

			file(GLOB_RECURSE DYLIBS \"\${APP_BUNDLE}/Contents/libs/*.dylib\")

			set(ALL_BINARIES \${EXECUTABLES} \${DYLIBS})

			message(STATUS \"Generating debug symbols: \${DSYM_BUNDLE}\")
			execute_process(
				COMMAND xcrun dsymutil
				-o \"\${DSYM_BUNDLE}\"
				\${ALL_BINARIES}
				RESULT_VARIABLE DSYM_RESULT
				OUTPUT_VARIABLE DSYM_OUT
				ERROR_VARIABLE DSYM_ERR
			)

			if(NOT DSYM_RESULT EQUAL 0)
				message(FATAL_ERROR \"Failed to generate dSYM: \${DSYM_ERR}\")
			endif()
		endif()
	")

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

# TODO ! ....

add_custom_command(
	COMMAND ${PYTHON} ${CMAKE_SOURCE_DIR}/scripts/package.py --skip_binaries --copy_to_build_folder --build_folder "${BUILD_FOLDER}" --keep_existing_files
	OUTPUT copy_resources_if_not_exist.stamp
)
add_custom_target(copy_resources_if_not_exist DEPENDS copy_resources_if_not_exist.stamp)
set_source_files_properties(copy_resources_if_not_exist.stamp PROPERTIES SYMBOLIC 1)

add_custom_command(
	COMMAND ${PYTHON} ${CMAKE_SOURCE_DIR}/scripts/package.py --skip_binaries --copy_to_build_folder --build_folder "${BUILD_FOLDER}"
	OUTPUT copy_resources.stamp
)
add_custom_target(copy_resources DEPENDS copy_resources.stamp)
set_source_files_properties(copy_resources.stamp PROPERTIES SYMBOLIC 1)

# TODO ! rm?
add_custom_command(
	COMMAND ${PYTHON} ${CMAKE_SOURCE_DIR}/scripts/package.py --build_folder "${BUILD_FOLDER}" --clean_first
	OUTPUT package.stamp
	DEPENDS ${zc_targets}
)
add_custom_target(package_zc DEPENDS package.stamp)
set_source_files_properties(package.stamp PROPERTIES SYMBOLIC 1)

add_custom_command(
	COMMAND ${PYTHON} ${CMAKE_SOURCE_DIR}/scripts/package.py --build_folder "${BUILD_FOLDER}" --clean_first --skip_archive
	OUTPUT package_no_archive.stamp
	DEPENDS ${zc_targets}
)
add_custom_target(package_zc_no_archive DEPENDS package_no_archive.stamp)
set_source_files_properties(package_no_archive.stamp PROPERTIES SYMBOLIC 1)

if(COPY_RESOURCES AND NOT EMSCRIPTEN)
	foreach(target IN LISTS zc_targets)
		add_dependencies(${target} copy_resources_if_not_exist)
	endforeach()
endif()
