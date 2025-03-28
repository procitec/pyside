# Copyright (C) 2023 The Qt Company Ltd.
# SPDX-License-Identifier: BSD-3-Clause

project(QtPositioning)

set(QtPositioning_OPTIONAL_SRC )
set(QtPositioning_DROPPED_ENTRIES )

set(QtPositioning_SRC
${QtPositioning_GEN_DIR}/qgeoaddress_wrapper.cpp
${QtPositioning_GEN_DIR}/qgeoareamonitorinfo_wrapper.cpp
${QtPositioning_GEN_DIR}/qgeoareamonitorsource_wrapper.cpp
${QtPositioning_GEN_DIR}/qgeolocation_wrapper.cpp
${QtPositioning_GEN_DIR}/qgeocircle_wrapper.cpp
${QtPositioning_GEN_DIR}/qgeocoordinate_wrapper.cpp
${QtPositioning_GEN_DIR}/qgeopath_wrapper.cpp
${QtPositioning_GEN_DIR}/qgeopolygon_wrapper.cpp
${QtPositioning_GEN_DIR}/qgeopositioninfo_wrapper.cpp
${QtPositioning_GEN_DIR}/qgeopositioninfosource_wrapper.cpp
${QtPositioning_GEN_DIR}/qgeopositioninfosourcefactory_wrapper.cpp
${QtPositioning_GEN_DIR}/qgeorectangle_wrapper.cpp
${QtPositioning_GEN_DIR}/qgeosatelliteinfo_wrapper.cpp
${QtPositioning_GEN_DIR}/qgeosatelliteinfosource_wrapper.cpp
${QtPositioning_GEN_DIR}/qgeoshape_wrapper.cpp
${QtPositioning_GEN_DIR}/qnmeapositioninfosource_wrapper.cpp
${QtPositioning_GEN_DIR}/qnmeasatelliteinfosource_wrapper.cpp
# module is always needed
${QtPositioning_GEN_DIR}/qtpositioning_module_wrapper.cpp
)

set(QtPositioning_include_dirs ${QtPositioning_SOURCE_DIR}
                               ${QtPositioning_BINARY_DIR}
                               ${Qt${QT_MAJOR_VERSION}Core_INCLUDE_DIRS}
                               ${Qt${QT_MAJOR_VERSION}Positioning_INCLUDE_DIRS}
                               ${libpyside_SOURCE_DIR}
                               ${QtCore_GEN_DIR})

set(QtPositioning_libraries pyside6
                            ${Qt${QT_MAJOR_VERSION}Positioning_LIBRARIES})

set(QtPositioning_deps QtCore)

create_pyside_module(NAME QtPositioning
                     INCLUDE_DIRS QtPositioning_include_dirs
                     LIBRARIES QtPositioning_libraries
                     DEPS QtPositioning_deps
                     TYPESYSTEM_PATH QtPositioning_SOURCE_DIR
                     SOURCES QtPositioning_SRC
                     DROPPED_ENTRIES QtPositioning_DROPPED_ENTRIES)

if (APPLE)
    # The QtLocation permission plugin cannot be linked to QtCore.abi3.so because for a framework
    # build of Qt, the QtCore framework bundle must be loaded before calling
    # Q_IMPORT_PLUGIN(QDarwinLocationPermissionPlugin)
    set(permission_plugin_name "QDarwinLocationPermissionPlugin")
    set(permission_plugin "${QT_CMAKE_EXPORT_NAMESPACE}::${permission_plugin_name}")
    set_target_properties(QtPositioning PROPERTIES "_qt_has_${permission_plugin_name}_usage_description" TRUE)
    # importing the plugin
    qt6_import_plugins(QtPositioning INCLUDE ${permission_plugin})
endif()
