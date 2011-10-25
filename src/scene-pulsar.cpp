/*
 * Copyright © 2008 Ben Smith
 * Copyright © 2010-2011 Linaro Limited
 *
 * This file is part of the glmark2 OpenGL (ES) 2.0 benchmark.
 *
 * glmark2 is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * glmark2 is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * glmark2.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *  Ben Smith (original glmark benchmark)
 *  Alexandros Frantzis (glmark2)
 *  Marc Ordinas i Llopis, Collabora Ltd. (pulsar scene)
 */
#include "scene.h"
#include "mat.h"
#include "stack.h"
#include "vec.h"
#include "log.h"
#include "program.h"
#include "shader-source.h"

#include <cmath>

ScenePulsar::ScenePulsar(Canvas &pCanvas) :
    Scene(pCanvas, "pulsar"),
    numQuads_(0),
    texture_(0)
{
    mOptions["quads"] = Scene::Option("quads", "5", "Number of quads to render");
    mOptions["texture"] = Scene::Option("texture", "false", "Enable texturing");
    mOptions["light"] = Scene::Option("light", "false", "Enable lighting");
    mOptions["random"] = Scene::Option("random", "false", "Enable random rotation speeds");
}

ScenePulsar::~ScenePulsar()
{
}

int ScenePulsar::load()
{
    scale_ = LibMatrix::vec3(1.0, 1.0, 1.0);

    mRunning = false;

    return 1;
}

void ScenePulsar::unload()
{
}

void ScenePulsar::setup()
{
    Scene::setup();

    // Disable back-face culling
    glDisable(GL_CULL_FACE);
    // Enable alpha blending
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Create a rotation for each quad.
    std::stringstream ss;
    ss << mOptions["quads"].value;
    ss >> numQuads_;

    srand((unsigned)time(0));
    for (int i = 0; i < numQuads_; i++) {
        rotations_.push_back(LibMatrix::vec3());
        if (mOptions["random"].value == "true") {
            rotationSpeeds_.push_back(LibMatrix::vec3(((float)rand() / (float)RAND_MAX) * 5.0,
                                                      ((float)rand() / (float)RAND_MAX) * 5.0,
                                                      0.0));
        }
        else {
            float integral;
            float x_rot = std::modf((i + 1) * M_PI, &integral);
            float y_rot = std::modf((i + 1) * M_E, &integral);
            rotationSpeeds_.push_back(LibMatrix::vec3(x_rot * 5.0,
                                                      y_rot * 5.0,
                                                      0.0));
        }
    }

    // Load shaders
    std::string vtx_shader_filename;
    std::string frg_shader_filename;
    static const LibMatrix::vec4 lightPosition(-20.0f, 20.0f,-20.0f, 1.0f);
    if (mOptions["light"].value == "true") {
        vtx_shader_filename = GLMARK_DATA_PATH"/shaders/pulsar-light.vert";
    } else {
        vtx_shader_filename = GLMARK_DATA_PATH"/shaders/pulsar.vert";
    }

    if (mOptions["texture"].value == "true") {
        frg_shader_filename = GLMARK_DATA_PATH"/shaders/light-basic-tex.frag";
        Texture::load(GLMARK_DATA_PATH"/textures/crate-base.png", &texture_,
                      GL_NEAREST, GL_NEAREST, 0);

    } else {
        frg_shader_filename = GLMARK_DATA_PATH"/shaders/light-basic.frag";
    }

    ShaderSource vtx_source(vtx_shader_filename);
    ShaderSource frg_source(frg_shader_filename);
    if (mOptions["light"].value == "true") {
        // Load the light position constant
        vtx_source.add_const("LightSourcePosition", lightPosition);
    }

    if (!Scene::load_shaders_from_strings(program_, vtx_source.str(),
                                          frg_source.str()))
    {
        return;
    }

    create_and_setup_mesh();

    program_.start();

    mCurrentFrame = 0;

    mRunning = true;
    mStartTime = Scene::get_timestamp_us() / 1000000.0;
    mLastUpdateTime = mStartTime;
}

void ScenePulsar::teardown()
{
    program_.stop();
    program_.release();

    if (mOptions["texture"].value == "true") {
        glDeleteTextures(1, &texture_);
        texture_ = 0;
    }

    // Re-enable back-face culling
    glEnable(GL_CULL_FACE);
    // Disable alpha blending
    glDisable(GL_BLEND);

    mesh_.reset();

    Scene::teardown();
}

void ScenePulsar::update()
{
    double current_time = Scene::get_timestamp_us() / 1000000.0;
    double dt = current_time - mLastUpdateTime;
    double elapsed_time = current_time - mStartTime;

    mLastUpdateTime = current_time;

    if (elapsed_time >= mDuration) {
        mAverageFPS = mCurrentFrame / elapsed_time;
        mRunning = false;
    }

    for (int i = 0; i < numQuads_; i++) {
        rotations_[i] += rotationSpeeds_[i] * (dt * 60);
    }

    scale_ = LibMatrix::vec3(cos(elapsed_time / 3.60) * 10.0, sin(elapsed_time / 3.60) * 10.0, 1.0);

    mCurrentFrame++;
}

void ScenePulsar::draw()
{
    if (mOptions["texture"].value == "true") {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture_);
    }

    for (int i = 0; i < numQuads_; i++) {
        // Load the ModelViewProjectionMatrix uniform in the shader
        LibMatrix::Stack4 model_view;
        LibMatrix::mat4 model_view_proj(mCanvas.projection());
        model_view.scale(scale_.x(), scale_.y(), scale_.z());
        model_view.translate(0.0f, 0.0f, -10.0f);
        model_view.rotate(rotations_[i].x(), 1.0f, 0.0f, 0.0f);
        model_view.rotate(rotations_[i].y(), 0.0f, 1.0f, 0.0f);
        model_view.rotate(rotations_[i].z(), 0.0f, 0.0f, 1.0f);
        model_view_proj *= model_view.getCurrent();
        program_["ModelViewProjectionMatrix"] = model_view_proj;

        if (mOptions["light"].value == "true") {
            // Load the NormalMatrix uniform in the shader. The NormalMatrix is the
            // inverse transpose of the model view matrix.
            LibMatrix::mat4 normal_matrix(model_view.getCurrent());
            normal_matrix.inverse().transpose();
            program_["NormalMatrix"] = normal_matrix;
        }

        mesh_.render_vbo();
    }
}

Scene::ValidationResult
ScenePulsar::validate()
{
    return ValidationUnknown;
}

void ScenePulsar::create_and_setup_mesh()
{
    bool texture = mOptions["texture"].value == "true";
    bool light = mOptions["light"].value == "true";

    struct PlaneMeshVertex {
        LibMatrix::vec3 position;
        LibMatrix::vec4 color;
        LibMatrix::vec2 texcoord;
        LibMatrix::vec3 normal;
    };

    PlaneMeshVertex plane_vertices[] = {
        {
          LibMatrix::vec3(-1.0, -1.0, 0.0),
          LibMatrix::vec4(1.0, 0.0, 0.0, 0.4),
          LibMatrix::vec2(0.0, 0.0),
          LibMatrix::vec3(0.0, 0.0, 1.0)
        },
        {
          LibMatrix::vec3(-1.0, 1.0, 0.0),
          LibMatrix::vec4(0.0, 1.0, 0.0, 0.4),
          LibMatrix::vec2(0.0, 1.0),
          LibMatrix::vec3(0.0, 0.0, 1.0)
        },
        {
          LibMatrix::vec3(1.0, 1.0, 0.0),
          LibMatrix::vec4(0.0, 0.0, 1.0, 0.4),
          LibMatrix::vec2(1.0, 1.0),
          LibMatrix::vec3(0.0, 0.0, 1.0)
        },
        {
          LibMatrix::vec3(1.0, -1.0, 0.0),
          LibMatrix::vec4(1.0, 1.0, 1.0, 1.0),
          LibMatrix::vec2(1.0, 0.0),
          LibMatrix::vec3(0.0, 0.0, 1.0)
        }
    };

    unsigned int vertex_index[] = {0, 1, 2, 0, 2, 3};

    // Set vertex format
    std::vector<int> vertex_format;
    vertex_format.push_back(3);     // Position
    vertex_format.push_back(4);     // Color
    if (texture)
        vertex_format.push_back(2); // Texcoord
    if (light)
        vertex_format.push_back(3); // Normal

    mesh_.set_vertex_format(vertex_format);

    // Build the plane mesh
    for (size_t i = 0; i < sizeof(vertex_index) / sizeof(*vertex_index); i++) {
        PlaneMeshVertex& vertex = plane_vertices[vertex_index[i]];

        mesh_.next_vertex();
        mesh_.set_attrib(0, vertex.position);
        mesh_.set_attrib(1, vertex.color);
        if (texture)
            mesh_.set_attrib(2, vertex.texcoord);
        if (light)
            mesh_.set_attrib(2 + ((int)texture), vertex.normal);
    }

    mesh_.build_vbo();

    // Set attribute locations
    std::vector<GLint> attrib_locations;
    attrib_locations.push_back(program_["position"].location());
    attrib_locations.push_back(program_["vtxcolor"].location());
    if (texture)
        attrib_locations.push_back(program_["texcoord"].location());
    if (light)
        attrib_locations.push_back(program_["normal"].location());
    mesh_.set_attrib_locations(attrib_locations);
}

