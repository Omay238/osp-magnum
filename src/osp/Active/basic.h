/**
 * Open Space Program
 * Copyright © 2019-2021 Open Space Program Project
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
#pragma once

#include "../types.h"
#include "activetypes.h"

#include <longeron/id_management/null.hpp>

#include <string>

namespace osp::active
{

/**
 * @brief Component for transformation (in meters)
 */
struct ACompTransform
{
    osp::Matrix4 m_transform;
};

// TODO: this scheme of controlled and mutable likely isn't the best, maybe
//       consider other options

/**
 * @brief Indicates that an entity's ACompTransform is owned by some specific
 *        system, and shouldn't be modified freely.
 *
 * This can be used by a physics or animation system, which may set the
 * transform each frame.
 */
struct ACompTransformControlled { };

/**
 * @brief Allows mutation for entities with ACompTransformControlled, as long as
 *        a dirty flag is set.
 */
struct ACompTransformMutable{ bool m_dirty{false}; };

/**
 * @brief The ACompFloatingOrigin struct
 */
struct ACompFloatingOrigin { };

/**
 * @brief Simple name component
 */
struct ACompName
{
    std::string m_name;
};

using TreePos_t = uint32_t;

struct ACtxSceneGraph
{
    // Tree structure stored using an array of descendant count in parallel with
    // identificaton (entities)
    // A(B(C(D)), E(F(G(H,I)))) -> [A,B,C,D,E,F,G,H,I] and [8,2,1,0,4,3,2,0,0]
    std::vector<ActiveEnt>  m_treeToEnt{lgrn::id_null<ActiveEnt>()};
    std::vector<uint32_t>   m_treeDescendants{std::initializer_list<uint32_t>{0}};

    std::vector<ActiveEnt>  m_entParent;
    std::vector<TreePos_t>  m_entToTreePos;

    std::vector<TreePos_t>  m_delete;

    void resize(std::size_t ents)
    {
        m_treeToEnt         .reserve(ents);
        m_treeDescendants   .reserve(ents);
        m_entParent         .resize(ents);
        m_entToTreePos      .resize(ents);
    }
};

/**
 * @brief Storage for basic components
 */
struct ACtxBasic
{
    ACtxSceneGraph m_scnGraph;

    acomp_storage_t<ACompTransform>             m_transform;
    acomp_storage_t<ACompTransformControlled>   m_transformControlled;
    acomp_storage_t<ACompTransformMutable>      m_transformMutable;
    acomp_storage_t<ACompFloatingOrigin>        m_floatingOrigin;
    acomp_storage_t<ACompName>                  m_name;
};

template<typename IT_T>
void update_delete_basic(ACtxBasic &rCtxBasic, IT_T first, IT_T const& last)
{
    rCtxBasic.m_floatingOrigin  .remove(first, last);
    rCtxBasic.m_name            .remove(first, last);

    while (first != last)
    {
        ActiveEnt const ent = *first;

        if (rCtxBasic.m_transform.contains(ent))
        {
            rCtxBasic.m_transform           .remove(ent);
            rCtxBasic.m_transformControlled .remove(ent);
            rCtxBasic.m_transformMutable    .remove(ent);

        }

        std::advance(first, 1);
    }
}

} // namespace osp::active
