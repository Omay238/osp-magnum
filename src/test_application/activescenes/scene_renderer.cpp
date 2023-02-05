/**
 * Open Space Program
 * Copyright © 2019-2022 Open Space Program Project
 *
 * MIT License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#include "scene_renderer.h"
#include "scenarios.h"
#include "identifiers.h"
#include "../ActiveApplication.h"

#include <Magnum/GL/DefaultFramebuffer.h>
#include <osp/Active/SysRender.h>
#include <osp/Active/SysSceneGraph.h>
#include <osp/Active/opengl/SysRenderGL.h>
#include <osp/Shaders/MeshVisualizer.h>
#include <osp/Shaders/Flat.h>
#include <osp/Shaders/Phong.h>

#include <osp/unpack.h>

using namespace osp;
using namespace osp::shader;
using namespace osp::active;

using osp::input::UserInputHandler;

namespace testapp::scenes
{

Session setup_magnum_application(
        Builder_t&                      rBuilder,
        ArrayView<entt::any> const      topData,
        Tags&                           rTags,
        TopDataId const                 idResources,
        ActiveApplication::Arguments    args)
{
    Session magnum;
    OSP_SESSION_ACQUIRE_DATA(magnum, topData,   TESTAPP_APP_MAGNUM);
    OSP_SESSION_ACQUIRE_TAGS(magnum, rTags,     TESTAPP_APP_MAGNUM);

    // Order-dependent; ActiveApplication construction starts OpenGL context
    auto &rUserInput    = osp::top_emplace<UserInputHandler>(topData, idUserInput, 12);
    osp::top_emplace<ActiveApplication>                     (topData, idActiveApp, args, rUserInput);
    auto &rRenderGl     = osp::top_emplace<RenderGL>        (topData, idRenderGl);

    config_controls(rUserInput);
    SysRenderGL::setup_context(rRenderGl);

    magnum.task() = rBuilder.task().assign({tgCleanupMagnumEvt, tgGlUse}).data(
            "Clean up Magnum renderer",
            TopDataIds_t{           idResources,          idRenderGl},
            wrap_args([] (Resources& rResources, RenderGL& rRenderGl) noexcept
    {
        SysRenderGL::clear_resource_owners(rRenderGl, rResources);
        rRenderGl = {}; // Needs the OpenGL thread for destruction
    }));
    magnum.m_tgCleanupEvt = tgCleanupMagnumEvt;

    return magnum;
}


Session setup_scene_renderer(
        Builder_t&                  rBuilder,
        ArrayView<entt::any> const  topData,
        Tags&                       rTags,
        Session const&              magnum,
        Session const&              scnCommon,
        TopDataId const             idResources)
{
    OSP_SESSION_UNPACK_TAGS(scnCommon,  TESTAPP_COMMON_SCENE);
    OSP_SESSION_UNPACK_DATA(scnCommon,  TESTAPP_COMMON_SCENE);
    OSP_SESSION_UNPACK_TAGS(magnum,     TESTAPP_APP_MAGNUM);
    OSP_SESSION_UNPACK_DATA(magnum,     TESTAPP_APP_MAGNUM);

    Session renderer;
    OSP_SESSION_ACQUIRE_DATA(renderer, topData, TESTAPP_COMMON_RENDERER);
    OSP_SESSION_ACQUIRE_TAGS(renderer, rTags,   TESTAPP_COMMON_RENDERER);

    top_emplace< ACtxSceneRenderGL >    (topData, idScnRender);
    top_emplace< RenderGroup >          (topData, idGroupFwd);

    auto &rCamera = top_emplace< Camera >(topData, idCamera);

    rCamera.m_far = 1u << 24;
    rCamera.m_near = 1.0f;
    rCamera.m_fov = Magnum::Deg(45.0f);

    rBuilder.tag(tgDrawGlMod)           .depend_on({tgDrawGlDel});
    rBuilder.tag(tgDrawGlReq)           .depend_on({tgDrawGlDel, tgDrawGlMod});
    rBuilder.tag(tgMeshGlReq)           .depend_on({tgMeshGlMod});
    rBuilder.tag(tgTexGlReq)            .depend_on({tgTexGlMod});
    rBuilder.tag(tgEntTexReq)           .depend_on({tgEntTexMod});
    rBuilder.tag(tgEntMeshReq)          .depend_on({tgEntMeshMod});
    rBuilder.tag(tgCameraReq)           .depend_on({tgCameraMod});
    rBuilder.tag(tgGroupFwdMod)         .depend_on({tgGroupFwdDel});
    rBuilder.tag(tgGroupFwdReq)         .depend_on({tgGroupFwdDel, tgGroupFwdMod});
    rBuilder.tag(tgDrawTransformNew)    .depend_on({tgDrawTransformDel});
    rBuilder.tag(tgDrawTransformMod)    .depend_on({tgDrawTransformDel, tgDrawTransformNew});
    rBuilder.tag(tgDrawTransformReq)    .depend_on({tgDrawTransformDel, tgDrawTransformNew, tgDrawTransformMod});


    renderer.task() = rBuilder.task().assign({tgSyncEvt, tgGlUse, tgTexGlMod, tgTexGlMod}).data(
            "Synchronize used mesh and texture Resources with GL",
            TopDataIds_t{                      idDrawingRes,                idResources,          idRenderGl},
            wrap_args([] (ACtxDrawingRes const& rDrawingRes, osp::Resources& rResources, RenderGL& rRenderGl) noexcept
    {
        SysRenderGL::sync_scene_resources(rDrawingRes, rResources, rRenderGl);
    }));

    renderer.task() = rBuilder.task().assign({tgSyncEvt, tgTexGlReq, tgEntTexMod}).data(
            "Assign GL textures to entities with scene textures",
            TopDataIds_t{             idDrawing,                idDrawingRes,                   idScnRender,          idRenderGl},
            wrap_args([] (ACtxDrawing& rDrawing, ACtxDrawingRes& rDrawingRes, ACtxSceneRenderGL& rScnRender, RenderGL& rRenderGl) noexcept
    {
        SysRenderGL::assign_textures(rDrawing.m_diffuseTex, rDrawingRes.m_texToRes, rDrawing.m_diffuseDirty, rScnRender.m_diffuseTexId, rRenderGl);
    }));

    renderer.task() = rBuilder.task().assign({tgSyncEvt, tgTexGlReq, tgEntMeshMod, tgMeshReq}).data(
            "Assign GL meshes to entities with scene meshes",
            TopDataIds_t{             idDrawing,                idDrawingRes,                   idScnRender,          idRenderGl},
            wrap_args([] (ACtxDrawing& rDrawing, ACtxDrawingRes& rDrawingRes, ACtxSceneRenderGL& rScnRender, RenderGL& rRenderGl) noexcept
    {
        SysRenderGL::assign_meshes(rDrawing.m_mesh, rDrawingRes.m_meshToRes, rDrawing.m_meshDirty, rScnRender.m_meshId, rRenderGl);
    }));

    // TODO: Separate forward renderer into different tasks to allow other
    //       rendering techniques

    renderer.task() = rBuilder.task().assign({tgRenderEvt, tgGlUse, tgDrawTransformReq, tgGroupFwdReq, tgDrawReq, tgCameraReq, tgEntTexMod, tgEntMeshMod}).data(
            "Render and display scene",
            TopDataIds_t{                   idDrawing,          idRenderGl,                   idGroupFwd,              idCamera},
            wrap_args([] (ACtxDrawing const& rDrawing, RenderGL& rRenderGl, RenderGroup const& rGroupFwd, Camera const& rCamera) noexcept
    {
        using Magnum::GL::Framebuffer;
        using Magnum::GL::FramebufferClear;
        using Magnum::GL::Texture2D;

        // Bind offscreen FBO
        Framebuffer &rFbo = rRenderGl.m_fbo;
        rFbo.bind();

        // Clear it
        rFbo.clear( FramebufferClear::Color | FramebufferClear::Depth
                    | FramebufferClear::Stencil);


        ViewProjMatrix viewProj{rCamera.m_transform.inverted(), rCamera.perspective()};

        // Forward Render fwd_opaque group to FBO
        SysRenderGL::render_opaque(rGroupFwd, rDrawing.m_visible, viewProj);

        Texture2D &rFboColor = rRenderGl.m_texGl.get(rRenderGl.m_fboColor);
        SysRenderGL::display_texture(rRenderGl, rFboColor);
    }));


    renderer.task() = rBuilder.task().assign({tgSyncEvt, tgDelTotalReq, tgDrawGlDel}).data(
            "Delete GL components",
            TopDataIds_t{                   idScnRender,                   idDelTotal},
            wrap_args([] (ACtxSceneRenderGL& rScnRender, EntVector_t const& rDelTotal) noexcept
    {
        SysRenderGL::update_delete(rScnRender, std::cbegin(rDelTotal), std::cend(rDelTotal));
    }));

    renderer.task() = rBuilder.task().assign({tgSyncEvt, tgHierReq, tgTransformReq, tgDrawTransformMod}).data(
            "Calculate draw transforms",
            TopDataIds_t{                 idBasic,                   idDrawing,                   idScnRender},
            wrap_args([] (ACtxBasic const& rBasic, ACtxDrawing const& rDrawing, ACtxSceneRenderGL& rScnRender) noexcept
    {
        auto rootChildren = SysSceneGraph::children(rBasic.m_scnGraph);
        SysRender::update_draw_transforms(rBasic.m_scnGraph, rBasic.m_transform, rScnRender.m_drawTransform, rDrawing.m_drawable, std::begin(rootChildren), std::end(rootChildren));
    }));

    renderer.task() = rBuilder.task().assign({tgSyncEvt, tgDelTotalReq, tgGroupFwdDel}).data(
            "Delete entities from render groups",
            TopDataIds_t{             idGroupFwd,                idDelTotal},
            wrap_args([] (RenderGroup& rGroup, EntVector_t const& rDelTotal) noexcept
    {
        rGroup.m_entities.remove(std::cbegin(rDelTotal), std::cend(rDelTotal));
    }));

    return renderer;
}

Session setup_shader_visualizer(
        Builder_t&                  rBuilder,
        ArrayView<entt::any> const  topData,
        Tags&                       rTags,
        Session const&              magnum,
        Session const&              scnCommon,
        Session const&              scnRender,
        Session const&              material)
{
    OSP_SESSION_UNPACK_TAGS(scnCommon,  TESTAPP_COMMON_SCENE);
    OSP_SESSION_UNPACK_DATA(scnCommon,  TESTAPP_COMMON_SCENE);
    OSP_SESSION_UNPACK_DATA(magnum,     TESTAPP_APP_MAGNUM);
    OSP_SESSION_UNPACK_TAGS(scnRender,  TESTAPP_COMMON_RENDERER);
    OSP_SESSION_UNPACK_DATA(scnRender,  TESTAPP_COMMON_RENDERER);
    OSP_SESSION_UNPACK_TAGS(material,   TESTAPP_MATERIAL);
    OSP_SESSION_UNPACK_DATA(material,   TESTAPP_MATERIAL);
    auto &rScnRender    = top_get< ACtxSceneRenderGL >  (topData, idScnRender);
    auto &rRenderGl     = top_get< RenderGL >           (topData, idRenderGl);

    Session visualizer;
    OSP_SESSION_ACQUIRE_DATA(visualizer, topData, TESTAPP_SHADER_VISUALIZER)
    auto &rDrawVisual = top_emplace< ACtxDrawMeshVisualizer >(topData, idDrawVisual);

    rDrawVisual.m_shader = MeshVisualizer{ MeshVisualizer::Configuration{}.setFlags(MeshVisualizer::Flag::Wireframe) };
    rDrawVisual.assign_pointers(rScnRender, rRenderGl);

    visualizer.task() = rBuilder.task().assign({tgSyncEvt, tgMatReq, tgGroupFwdMod}).data(
            "Sync MeshVisualizer shader entities",
            TopDataIds_t{                   idMatDirty,                idMatEnts,             idGroupFwd,                     idDrawVisual},
            wrap_args([] (EntVector_t const& rMatDirty, EntSet_t const& rMatEnts, RenderGroup& rGroup, ACtxDrawMeshVisualizer& rVisualizer) noexcept
    {
        sync_visualizer(std::cbegin(rMatDirty), std::cend(rMatDirty), rMatEnts, rGroup.m_entities, rVisualizer);
    }));

    visualizer.task() = rBuilder.task().assign({tgSyncEvt, tgMatReq, tgHierReq, tgTransformReq, tgDrawTransformNew}).data(
            "Add draw transforms to mesh visualizer",
            TopDataIds_t{                   idMatDirty,                   idScnRender},
            wrap_args([] (EntVector_t const& rMatDirty, ACtxSceneRenderGL& rScnRender) noexcept
    {
        SysRender::assure_draw_transforms(rScnRender.m_drawTransform, std::cbegin(rMatDirty), std::cend(rMatDirty));
    }));


    return visualizer;
}


// TODO
#if 0

Session setup_shader_phong()
{
    // Setup Phong shaders
    auto const texturedFlags        = Phong::Flag::DiffuseTexture | Phong::Flag::AlphaMask | Phong::Flag::AmbientTexture;
    rDrawPhong.m_shaderDiffuse      = Phong{texturedFlags, 2};
    rDrawPhong.m_shaderUntextured   = Phong{{}, 2};
    rDrawPhong.assign_pointers(rScnRender, rRenderGl);

}

#endif

}

