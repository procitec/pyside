# Copyright (C) 2023 The Qt Company Ltd.
# SPDX-License-Identifier: BSD-3-Clause

project(Qt3DAnimation)

set(Qt3DAnimation_SRC
${Qt3DAnimation_GEN_DIR}/qt3danimation_wrapper.cpp
${Qt3DAnimation_GEN_DIR}/qt3danimation_qabstractanimation_wrapper.cpp
${Qt3DAnimation_GEN_DIR}/qt3danimation_qabstractanimationclip_wrapper.cpp
${Qt3DAnimation_GEN_DIR}/qt3danimation_qabstractchannelmapping_wrapper.cpp
${Qt3DAnimation_GEN_DIR}/qt3danimation_qabstractclipanimator_wrapper.cpp
${Qt3DAnimation_GEN_DIR}/qt3danimation_qabstractclipblendnode_wrapper.cpp
${Qt3DAnimation_GEN_DIR}/qt3danimation_qadditiveclipblend_wrapper.cpp
${Qt3DAnimation_GEN_DIR}/qt3danimation_qanimationaspect_wrapper.cpp
${Qt3DAnimation_GEN_DIR}/qt3danimation_qanimationcallback_wrapper.cpp
${Qt3DAnimation_GEN_DIR}/qt3danimation_qanimationclip_wrapper.cpp
${Qt3DAnimation_GEN_DIR}/qt3danimation_qanimationclipdata_wrapper.cpp
${Qt3DAnimation_GEN_DIR}/qt3danimation_qanimationcliploader_wrapper.cpp
${Qt3DAnimation_GEN_DIR}/qt3danimation_qanimationcontroller_wrapper.cpp
${Qt3DAnimation_GEN_DIR}/qt3danimation_qanimationgroup_wrapper.cpp
${Qt3DAnimation_GEN_DIR}/qt3danimation_qblendedclipanimator_wrapper.cpp
${Qt3DAnimation_GEN_DIR}/qt3danimation_qcallbackmapping_wrapper.cpp
${Qt3DAnimation_GEN_DIR}/qt3danimation_qchannel_wrapper.cpp
${Qt3DAnimation_GEN_DIR}/qt3danimation_qchannelcomponent_wrapper.cpp
${Qt3DAnimation_GEN_DIR}/qt3danimation_qchannelmapper_wrapper.cpp
${Qt3DAnimation_GEN_DIR}/qt3danimation_qchannelmapping_wrapper.cpp
${Qt3DAnimation_GEN_DIR}/qt3danimation_qclipanimator_wrapper.cpp
${Qt3DAnimation_GEN_DIR}/qt3danimation_qclipblendvalue_wrapper.cpp
${Qt3DAnimation_GEN_DIR}/qt3danimation_qclock_wrapper.cpp
${Qt3DAnimation_GEN_DIR}/qt3danimation_qkeyframe_wrapper.cpp
${Qt3DAnimation_GEN_DIR}/qt3danimation_qkeyframeanimation_wrapper.cpp
${Qt3DAnimation_GEN_DIR}/qt3danimation_qlerpclipblend_wrapper.cpp
${Qt3DAnimation_GEN_DIR}/qt3danimation_qmorphinganimation_wrapper.cpp
${Qt3DAnimation_GEN_DIR}/qt3danimation_qmorphtarget_wrapper.cpp
${Qt3DAnimation_GEN_DIR}/qt3danimation_qskeletonmapping_wrapper.cpp
${Qt3DAnimation_GEN_DIR}/qt3danimation_qvertexblendanimation_wrapper.cpp
# module is always needed
${Qt3DAnimation_GEN_DIR}/qt3danimation_module_wrapper.cpp)

set(Qt3DAnimation_include_dirs
                     ${Qt3DAnimation_SOURCE_DIR}
                     ${Qt3DAnimation_BINARY_DIR}
                     ${pyside6_SOURCE_DIR}
                     ${Qt${QT_MAJOR_VERSION}Core_INCLUDE_DIRS}
                     ${Qt${QT_MAJOR_VERSION}Gui_INCLUDE_DIR}
                     ${Qt${QT_MAJOR_VERSION}OpenGL_INCLUDE_DIRS}
                     ${Qt${QT_MAJOR_VERSION}3DCore_INCLUDE_DIRS}
                     ${Qt${QT_MAJOR_VERSION}3DRender_INCLUDE_DIRS}
                     ${Qt${QT_MAJOR_VERSION}3DAnimation_INCLUDE_DIRS}
                     ${libpyside_SOURCE_DIR}
                     ${QtCore_GEN_DIR}
                     ${QtGui_GEN_DIR}
                     ${QtOpenGL_GEN_DIR}
                     ${Qt3DCore_GEN_DIR}
                     ${Qt3DRender_GEN_DIR}
                     ${Qt3DAnimation_GEN_DIR})

set(Qt3DAnimation_libraries pyside6
                     ${Qt${QT_MAJOR_VERSION}3DAnimation_LIBRARIES})

set(Qt3DAnimation_deps Qt3DRender)

create_pyside_module(NAME Qt3DAnimation
                     INCLUDE_DIRS Qt3DAnimation_include_dirs
                     LIBRARIES Qt3DAnimation_libraries
                     DEPS Qt3DAnimation_deps
                     TYPESYSTEM_PATH Qt3DAnimation_SOURCE_DIR
                     SOURCES Qt3DAnimation_SRC
                     TYPESYSTEM_NAME ${Qt3DAnimation_BINARY_DIR}/typesystem_3danimation.xml)
