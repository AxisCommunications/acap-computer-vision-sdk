# --- AXIS VDO ---

if(NOT HAVE_AXIS_VDO AND PKG_CONFIG_FOUND)
  pkg_check_modules(AXIS_VDO capaxisvdo)

  if(AXIS_VDO_FOUND)
    set(HAVE_AXIS_VDO TRUE)

    set(CAPTURE_INCLUDE_DIRS
      ${AXIS_VDO_INCLUDE_DIRS})
    set(CAPTURE_LIBRARIES
      ${AXIS_VDO_LIBRARIES})
  endif()
endif()

if(HAVE_AXIS_VDO)
  ocv_add_external_target(axis_vdo "${CAPTURE_INCLUDE_DIRS}" "${CAPTURE_LIBRARIES}" "HAVE_AXIS_VDO")
endif()

set(HAVE_AXIS_VDO ${HAVE_AXIS_VDO} PARENT_SCOPE)
