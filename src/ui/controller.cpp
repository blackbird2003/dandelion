#include "controller.h"

#include <algorithm>
#include <string>
#include <vector>
#include <array>

#include <Eigen/Core>
#include <Eigen/Geometry>
#ifdef _WIN32
#include <Windows.h>
#endif
#include <glad/glad.h>
#include <imgui/imgui.h>

#include "../utils/math.hpp"
#include "../utils/ray.h"
#include "../utils/rendering.hpp"
#include "../utils/logger.h"

using Eigen::Matrix3f;
using Eigen::Matrix4f;
using Eigen::Quaternionf;
using Eigen::Vector3f;
using Eigen::Vector4f;
using std::array;
using std::get_if;
using std::make_unique;
using std::monostate;
using std::optional;
using std::sqrt;
using std::unique_ptr;
using std::visit;

Controller& Controller::controller()
{
    static Controller instance;
    return instance;
}

Controller::Controller()
    : highlighted_halfedge("highlighted halfedge", GL::Mesh::highlight_wireframe_color),
      picking_ray("picking ray")
{
    logger                         = get_logger("Controller");
    scene                          = make_unique<Scene>();
    menubar                        = make_unique<UI::Menubar>(debug_options);
    toolbar                        = make_unique<UI::Toolbar>(mode, selected_element);
    toolbar->on_element_selected   = [this](SelectableType element) { select(element); };
    toolbar->on_selection_canceled = [this]() { unselect(); };
    main_camera = make_unique<Camera>(Vector3f(1.0f, 2.0f, 3.0f), Vector3f(0.0f, 0.0f, 0.0f), 0.1f,
                                      1000.0f, 45.0f, 0.75f);
    trackball_radius = 300.0f;
    selected_element = monostate();
    // Device-independent configurations (i.e. styles) here.
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors    = style.Colors;
    // The following color platte is generated by Dear ImGui style editor, do not
    // modify them manually.
    colors[ImGuiCol_TextDisabled]          = ImVec4(0.84f, 0.83f, 0.86f, 1.00f);
    colors[ImGuiCol_WindowBg]              = ImVec4(0.13f, 0.15f, 0.17f, 1.00f);
    colors[ImGuiCol_FrameBg]               = ImVec4(0.20f, 0.22f, 0.26f, 1.00f);
    colors[ImGuiCol_FrameBgHovered]        = ImVec4(0.30f, 0.33f, 0.39f, 1.00f);
    colors[ImGuiCol_FrameBgActive]         = ImVec4(0.30f, 0.33f, 0.39f, 1.00f);
    colors[ImGuiCol_TitleBg]               = ImVec4(0.13f, 0.15f, 0.17f, 1.00f);
    colors[ImGuiCol_TitleBgActive]         = ImVec4(0.20f, 0.22f, 0.26f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed]      = ImVec4(0.13f, 0.15f, 0.17f, 1.00f);
    colors[ImGuiCol_MenuBarBg]             = ImVec4(0.13f, 0.15f, 0.17f, 1.00f);
    colors[ImGuiCol_CheckMark]             = ImVec4(0.30f, 0.47f, 0.80f, 1.00f);
    colors[ImGuiCol_SliderGrab]            = ImVec4(0.30f, 0.47f, 0.80f, 1.00f);
    colors[ImGuiCol_SliderGrabActive]      = ImVec4(0.34f, 0.52f, 0.88f, 1.00f);
    colors[ImGuiCol_Button]                = ImVec4(0.20f, 0.22f, 0.26f, 1.00f);
    colors[ImGuiCol_ButtonHovered]         = ImVec4(0.30f, 0.33f, 0.39f, 1.00f);
    colors[ImGuiCol_ButtonActive]          = ImVec4(0.30f, 0.33f, 0.39f, 1.00f);
    colors[ImGuiCol_Header]                = ImVec4(0.20f, 0.22f, 0.26f, 1.00f);
    colors[ImGuiCol_HeaderHovered]         = ImVec4(0.30f, 0.33f, 0.39f, 1.00f);
    colors[ImGuiCol_HeaderActive]          = ImVec4(0.20f, 0.22f, 0.26f, 1.00f);
    colors[ImGuiCol_ResizeGrip]            = ImVec4(0.37f, 0.41f, 0.49f, 0.78f);
    colors[ImGuiCol_ResizeGripHovered]     = ImVec4(0.37f, 0.41f, 0.49f, 1.00f);
    colors[ImGuiCol_ResizeGripActive]      = ImVec4(0.37f, 0.41f, 0.49f, 1.00f);
    colors[ImGuiCol_Tab]                   = ImVec4(0.13f, 0.15f, 0.17f, 1.00f);
    colors[ImGuiCol_TabHovered]            = ImVec4(0.30f, 0.33f, 0.39f, 1.00f);
    colors[ImGuiCol_TabActive]             = ImVec4(0.30f, 0.33f, 0.39f, 1.00f);
    colors[ImGuiCol_TabUnfocused]          = ImVec4(0.13f, 0.15f, 0.17f, 1.00f);
    colors[ImGuiCol_TabUnfocusedActive]    = ImVec4(0.20f, 0.22f, 0.26f, 1.00f);
    colors[ImGuiCol_DockingPreview]        = ImVec4(0.34f, 0.52f, 0.88f, 1.00f);
    colors[ImGuiCol_TextSelectedBg]        = ImVec4(0.30f, 0.33f, 0.39f, 1.00f);
    colors[ImGuiCol_DragDropTarget]        = ImVec4(0.37f, 0.41f, 0.49f, 1.00f);
    colors[ImGuiCol_NavHighlight]          = ImVec4(0.30f, 0.33f, 0.39f, 1.00f);
    colors[ImGuiCol_NavWindowingHighlight] = ImVec4(0.37f, 0.41f, 0.49f, 1.00f);
}

Controller::~Controller()
{
}

void Controller::on_mouse_dragged(bool initial)
{
    bool is_middle_dragging = ImGui::IsMouseDragging(ImGuiMouseButton_Middle);
    bool is_alt_left_dragging =
        ImGui::IsKeyDown(ImGuiKey_ModAlt) && ImGui::IsMouseDragging(ImGuiMouseButton_Left);
    bool is_ctrl_left_dragging =
        ImGui::IsKeyDown(ImGuiKey_ModCtrl) && ImGui::IsMouseDragging(ImGuiMouseButton_Left);
    if (is_middle_dragging || is_alt_left_dragging) {
        on_rotating(initial);
    } else if (is_ctrl_left_dragging) {
        on_translating(initial);
    }
}

void Controller::on_picking()
{
    bool picking_enabled = check_picking_enabled(mode);
    if (!picking_enabled) {
        unselect();
        return;
    }
    Controller& controller = Controller::controller();
    ImGuiIO& io            = ImGui::GetIO();
    // Construct a view ray from the main_camera according to the clicked position.
    // If the ray intersects with any object, the intersected object will be picked.
    Ray ray = generate_ray((int)(controller.window_width), (int)(controller.window_height),
                           (int)(io.MousePos.x), (int)(io.MousePos.y), *(controller.main_camera),
                           controller.main_camera->far);
    if (mode == WorkingMode::MODEL) {
        pick_element(ray);
    } else {
        pick_object(ray);
    }

    if (debug_options.show_picking_ray) {
        picking_ray.clear();
        picking_ray.add_line_segment(ray.origin, ray.origin + 1000.0f * ray.direction);
        picking_ray.to_gpu();
    }
}

void Controller::on_wheel_scrolled()
{
    ImGuiIO& io                = ImGui::GetIO();
    float input                = io.MouseWheel;
    unique_ptr<Camera>& camera = main_camera;
    Vector3f delta             = camera->position - camera->target;
    delta                      = std::pow(Controller::wheel_scroll_factor, input) * delta;
    camera->position           = camera->target + delta;
}

void Controller::on_framebuffer_resized(float width, float height)
{
    window_width              = width;
    window_height             = height;
    trackball_radius          = std::min(window_width, window_height) / 2.0f;
    main_camera->aspect_ratio = width / height;
}

void Controller::process_input()
{
    static bool dragging = false;
    ImGuiIO& io          = ImGui::GetIO();
    if (!io.WantCaptureMouse) {
        // io.WantCaptureMouse is false if the cursor is not hovering on any Dear ImGui widget.
        // Dragging with the left button and the alt key is equal as dragging
        // with the middle button.
        // bool is_middle_dragging = ImGui::IsMouseDragging(ImGuiMouseButton_Middle);
        // bool is_alt_left_dragging =
        //     ImGui::IsKeyDown(ImGuiKey_ModAlt) && ImGui::IsMouseDragging(ImGuiMouseButton_Left);
        bool is_dragging = ImGui::IsMouseDragging(ImGuiMouseButton_Left) ||
                           ImGui::IsMouseDragging(ImGuiMouseButton_Middle);
        if (is_dragging) {
            // Dragging indicates movement of the trackball.
            on_mouse_dragged(!dragging);
            if (!dragging) {
                // A new dragging path starts, clear the previous position.
                dragging = true;
            }
        } else {
            // Stop dragging.
            dragging = false;
        }
        if (!ImGui::IsKeyDown(ImGuiKey_ModAlt) && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            on_picking();
        }
        if (ImGui::IsKeyDown(ImGuiKey_MouseWheelY)) {
            on_wheel_scrolled();
        }
    }
    if (ImGui::IsKeyDown(ImGuiKey_Delete)) {
        Object** result = get_if<Object*>(&selected_element);
        if (result != nullptr) {
            Object* selected_object = *result;
            for (auto group = scene->groups.begin(); group != scene->groups.end(); ++group) {
                auto& objects = (*group)->objects;
                bool found    = false;
                for (auto object = objects.begin(); object != objects.end(); ++object) {
                    if (object->get() == selected_object) {
                        logger->info("delete object \"{}\" (ID: {}) from group \"{}\" (ID: {})",
                                     selected_object->name, selected_object->id, (*group)->name,
                                     (*group)->id);
                        objects.erase(object);
                        found = true;
                        break;
                    }
                }
                if (found) {
                    if (objects.empty()) {
                        logger->info("cascade to delete an empty group \"{}\" (ID: {})",
                                     (*group)->name, (*group)->id);
                        scene->groups.erase(group);
                    }
                    unselect();
                    break;
                }
            }
        }
        // allow removing light
        Light** light_result = get_if<Light*>(&selected_element);
        if (light_result != nullptr){
            Light *selected_light = *light_result;
            for (auto light = scene->lights.begin(); light != scene->lights.end(); ++light) {
                if (&(*light) == selected_light){
                    scene->lights.erase(light);
                    break;
                }
            }
        }
    }
    if (ImGui::IsKeyDown(ImGuiKey_ModCtrl) && ImGui::IsKeyDown(ImGuiKey_R)) {
        main_camera->position -= main_camera->target;
        main_camera->target = Vector3f(0.0f, 0.0f, 0.0f);
    }
}

void Controller::render(const Shader& shader)
{
    Controller& controller = Controller::controller();
    controller.menubar->render(*scene);
    controller.toolbar->render(*scene);

    ImGui::Render();

    Matrix4f view_projection =
        controller.main_camera->projection() * controller.main_camera->view();
    shader.set_uniform("view_projection", view_projection);
    shader.set_uniform("camera_position", controller.main_camera->position);
    controller.scene->render(shader, mode);

    render_selected_element(shader);
    render_debug_helpers(shader);
}

void Controller::select(SelectableType element)
{
    unselect();
    visit(overloaded{[]([[maybe_unused]] monostate empty) {},
                     [this](Object* object) { select_object(object); },
                     [this](const Halfedge* halfedge) { select_halfedge(halfedge); },
                     [this](Vertex* vertex) { select_vertex(vertex); },
                     [this](Edge* edge) { select_edge(edge); },
                     [this](Face* face) { select_face(face); },
                     [this](Light* light) { select_light(light); }},
          element);
}

void Controller::unselect()
{
    static auto unselect_object           = []([[maybe_unused]] Object* object) {};
    static auto clear_highlighted_element = [this]() {
        highlighted_element.clear();
        highlighted_element.to_gpu();
    };
    static auto unselect_halfedge = [this]([[maybe_unused]] const Halfedge* halfedge) {
        highlighted_halfedge.clear();
        highlighted_halfedge.to_gpu();
    };
    static auto unselect_vertex = []([[maybe_unused]] Vertex* vertex) {
        clear_highlighted_element();
    };
    static auto unselect_edge  = []([[maybe_unused]] Edge* edge) { clear_highlighted_element(); };
    static auto unselect_face  = []([[maybe_unused]] Face* face) { clear_highlighted_element(); };
    static auto unselect_light = []([[maybe_unused]] Light* light) { clear_highlighted_element(); };
    visit(overloaded{[]([[maybe_unused]] monostate empty) {}, unselect_halfedge, unselect_object,
                     unselect_vertex, unselect_edge, unselect_face, unselect_light},
          selected_element);
    if (mode != WorkingMode::MODEL) {
        scene->selected_object = nullptr;
    }
    if (scene->halfedge_mesh != nullptr) {
        scene->halfedge_mesh->inconsistent_element = monostate();
    }
    selected_element = monostate();
}

void Controller::render_selected_element(const Shader& shader)
{
    static auto render_mesh_element = [this, &shader](unsigned element_flag) {
        shader.set_uniform("color_per_vertex", false);
        shader.set_uniform("use_global_color", true);
        shader.set_uniform("model", I4f);
        glDisable(GL_DEPTH_TEST);
        highlighted_element.render(shader, element_flag, false,
                                   GL::Mesh::highlight_wireframe_color);
        glEnable(GL_DEPTH_TEST);
    };
    static auto render_halfedge = [this, &shader]([[maybe_unused]] const Halfedge* halfedge) {
        glDisable(GL_DEPTH_TEST);
        highlighted_halfedge.render(shader);
        glEnable(GL_DEPTH_TEST);
    };
    static auto render_vertex = [this](Vertex* vertex) {
        highlighted_element.vertices.update(0, vertex->pos);
        highlighted_element.vertices.to_gpu();
        render_mesh_element(GL::Mesh::vertices_flag);
    };
    static auto render_edge = [this](Edge* edge) {
        highlighted_element.vertices.update(0, edge->halfedge->from->pos);
        highlighted_element.vertices.update(1, edge->halfedge->inv->from->pos);
        highlighted_element.vertices.to_gpu();
        render_mesh_element(GL::Mesh::edges_flag);
    };
    static auto render_face = [this](Face* face) {
        const Halfedge* h = face->halfedge;
        size_t i          = 0;
        do {
            highlighted_element.vertices.update(i, h->from->pos);
            h = h->next;
            ++i;
        } while (h != face->halfedge);
        highlighted_element.vertices.to_gpu();
        render_mesh_element(GL::Mesh::faces_flag);
    };
    static auto render_light = [this, &shader](Light* light) {
        Eigen::Affine3f t;
        t              = Eigen::Translation3f(light->position);
        Matrix4f model = t.matrix();
        shader.set_uniform("color_per_vertex", false);
        shader.set_uniform("use_global_color", true);
        shader.set_uniform("model", model);
        glDisable(GL_DEPTH_TEST);
        highlighted_element.render(shader, GL::Mesh::vertices_flag, false,
                                   GL::Mesh::highlight_wireframe_color);
        glEnable(GL_DEPTH_TEST);
    };
    visit(overloaded{[]([[maybe_unused]] monostate empty) {},
                     []([[maybe_unused]] Object* object) {}, render_halfedge, render_vertex,
                     render_edge, render_face, render_light},
          selected_element);
}

void Controller::render_debug_helpers(const Shader& shader)
{
    if (debug_options.show_picking_ray) {
        shader.set_uniform("model", I4f);
        picking_ray.render(shader);
    }
    if (debug_options.show_BVH) {
        for (auto& group : scene->groups) {
            for (auto& object : group->objects) {
                shader.set_uniform("model", object->model());
                object->BVH_boxes.render(shader);
            }
        }
    }
}

void Controller::pick_object(Ray& ray)
{
    optional<Intersection> hit = std::nullopt;
    Object* hit_object         = nullptr;
    // Test all objects and maintain the minimal t value.
    for (auto& group : scene->groups) {
        for (auto& object : group->objects) {
            Matrix4f model                = object->model();
            GL::Mesh& mesh                = object->mesh;
            optional<Intersection> result = object->bvh->intersect(ray, mesh, model);
            if (!result.has_value()) {
                continue;
            }
            if (!hit.has_value() || hit.value().t > result.value().t) {
                hit        = result;
                hit_object = object.get();
            }
        }
    }
    if (hit.has_value()) {
        logger->debug("object {} (ID: {}) is picked", hit_object->name, hit_object->id);
        select(hit_object);
    } else {
        unselect();
    }
}

void Controller::pick_element(Ray& ray)
{
    if (scene->halfedge_mesh == nullptr) {
        return;
    }
    logger->debug("perform picking on object \"{}\" (ID: {})", scene->selected_object->name,
                  scene->selected_object->id);
    GL::Mesh& mesh             = scene->selected_object->mesh;
    optional<Intersection> hit = naive_intersect(ray, mesh, I4f);
    if (hit.has_value()) {
        size_t face_index        = hit.value().face_index;
        Vector3f& w              = hit.value().barycentric_coord;
        array<size_t, 3> indices = mesh.face(face_index);
        size_t max_weight_index  = 0;
        size_t min_weight_index  = 0;
        logger->debug("hit face {} with barycentric coordinates {:.3f}", face_index, w);
        for (size_t i = 1; i < 3; ++i) {
            if (w(i) > w(max_weight_index)) {
                max_weight_index = i;
            }
            if (w(i) < w(min_weight_index)) {
                min_weight_index = i;
            }
        }
        if (w(max_weight_index) > 0.9f) {
            const size_t selected_vertex_index = indices[max_weight_index];
            logger->debug("try to select vertex {}",
                          scene->halfedge_mesh->v_pointers[selected_vertex_index]->id);
            select(scene->halfedge_mesh->v_pointers[selected_vertex_index]);
            return;
        } else if (w(min_weight_index) < 0.1f) {
            const size_t index1 = indices[(min_weight_index + 1) % 3];
            const size_t index2 = indices[(min_weight_index + 2) % 3];
            Vertex* v1          = scene->halfedge_mesh->v_pointers[index1];
            Vertex* v2          = scene->halfedge_mesh->v_pointers[index2];
            Halfedge* h         = v1->halfedge;
            while (h->inv->from != v2) {
                h = h->inv->next;
            };
            if (w(min_weight_index) > 0.05f) {
                logger->debug("try to select halfedge {}", h->id);
                select(h);
            } else {
                logger->debug("try to select edge {}", h->edge->id);
                select(h->edge);
            }
            return;
        }
        Vertex* v1  = scene->halfedge_mesh->v_pointers[indices[0]];
        Vertex* v2  = scene->halfedge_mesh->v_pointers[indices[1]];
        Halfedge* h = v1->halfedge;
        while (h->inv->from != v2) {
            h = h->inv->next;
        };
        logger->debug("try to select face {}", h->face->id);
        select(h->face);
        return;
    }
    unselect();
}

void Controller::select_object(Object* object)
{
    selected_element       = object;
    scene->selected_object = object;
}

void Controller::select_halfedge(const Halfedge* halfedge)
{
    selected_element = halfedge;
    auto [from, to]  = HalfedgeMesh::halfedge_arrow_endpoints(halfedge);
    highlighted_halfedge.add_arrow(from, to);
    highlighted_halfedge.to_gpu();
}

void Controller::select_vertex(Vertex* vertex)
{
    selected_element                           = vertex;
    scene->halfedge_mesh->inconsistent_element = vertex;
    highlighted_element.vertices.append(vertex->pos.x(), vertex->pos.y(), vertex->pos.z());
    highlighted_element.to_gpu();
}

void Controller::select_edge(Edge* edge)
{
    selected_element                           = edge;
    scene->halfedge_mesh->inconsistent_element = edge;
    const Vertex* v1                           = edge->halfedge->from;
    const Vertex* v2                           = edge->halfedge->inv->from;
    highlighted_element.vertices.append(v1->pos.x(), v1->pos.y(), v1->pos.z());
    highlighted_element.vertices.append(v2->pos.x(), v2->pos.y(), v2->pos.z());
    highlighted_element.edges.append(0u, 1u);
    highlighted_element.to_gpu();
}

void Controller::select_face(Face* face)
{
    selected_element                           = face;
    scene->halfedge_mesh->inconsistent_element = face;
    const Halfedge* h                          = face->halfedge;
    const Vertex* v;
    size_t i = 0;
    do {
        v = h->from;
        highlighted_element.vertices.append(v->pos.x(), v->pos.y(), v->pos.z());
        highlighted_element.faces.data.push_back((unsigned int)i);
        h = h->next;
        ++i;
    } while (h != face->halfedge);
    highlighted_element.to_gpu();
}

void Controller::select_light(Light* light)
{
    selected_element = light;
    highlighted_element.vertices.append(0.0f, 0.0f, 0.0f);
    highlighted_element.vertices.append(0.1f, 0.0f, 0.0f);
    highlighted_element.vertices.append(-0.1f, 0.0f, 0.0f);
    highlighted_element.vertices.append(0.0f, 0.1f, 0.0f);
    highlighted_element.vertices.append(0.0f, -0.1f, 0.0f);
    highlighted_element.vertices.append(0.0f, 0.0f, 0.1f);
    highlighted_element.vertices.append(0.0f, 0.0f, -0.1f);
    highlighted_element.to_gpu();
}

void Controller::on_rotating(bool initial)
{
    static Vector3f previous_pos;
    Matrix4f inv_view       = main_camera->view().inverse();
    auto to_trackball_space = [this](const ImVec2& mouse_pos) -> Vector3f {
        // The mouse position starts from the top-left corner of window, x is greater at right and
        // y is greater at bottom.
        // To map the mouse position to the trackball space, first we invert y axis and move the
        // origin to the center of window.
        Vector3f track_pos(mouse_pos.x - window_width / 2.0f, -(mouse_pos.y - window_height / 2.0f),
                           0.0f);
        // Then calculate z coordinate according to x and y values. The trackball surface is
        // combination of sphere x^2 + y^2 + z^2 = r^2 and hyperboloid 4z^2(x^2 + y^2) = r^2.
        float condition = squ(track_pos.x()) + squ(track_pos.y());
        float r_squ     = squ(trackball_radius);
        if (condition < r_squ / 2.0f) {
            // In the inner area (close to the origin) it is sphere.
            track_pos.z() = sqrt(r_squ - condition);
        } else {
            // In the outer area it is hyperboloid.
            track_pos.z() = r_squ / (2.0f * sqrt(condition));
        }
        return track_pos;
    };
    ImGuiIO& io = ImGui::GetIO();
    if (initial) {
        previous_pos = to_trackball_space(io.MousePos);
        return;
    }
    Vector3f pos = to_trackball_space(io.MousePos);
    // Here we construct a rotation which rotates the camera from `pos` to `previous_pos`. Because
    // we want to rotate the scene from `pos` to `previous_pos`, and the rotation of camera is
    // the inverted rotation of scene.
    // The frame (i.e. axis) of trackball space is same as view space, because the xOy plane is
    // always placed towards the viewer (camera). We want to rotate the camera in world space, so
    // all vectors should be re-projected to world space by multiplying the inverse of view matrix.
    Quaternionf quat =
        Quaternionf::FromTwoVectors((inv_view * pos.homogeneous()).hnormalized(),
                                    (inv_view * previous_pos.homogeneous()).hnormalized());
    Vector3f& target             = main_camera->target;
    Vector3f previous_camera_pos = main_camera->position;
    Vector3f new_camera_pos      = quat * (previous_camera_pos - target) + target;
    bool x_inverted =
        (new_camera_pos.x() - target.x()) * (previous_camera_pos.x() - target.x()) < 0.0f;
    bool z_inverted =
        (new_camera_pos.z() - target.z()) * (previous_camera_pos.z() - target.z()) < 0.0f;
    if (x_inverted && z_inverted) {
        main_camera->world_up.y() *= -1.0f;
        logger->debug("world up inverted");
    }
    main_camera->position = new_camera_pos;
    previous_pos          = pos;
}

void Controller::on_translating(bool initial)
{
    static ImVec2 previous_pos;
    ImGuiIO& io = ImGui::GetIO();
    if (initial) {
        previous_pos = io.MousePos;
        return;
    }
    Matrix4f inv_view    = main_camera->view().inverse();
    const Vector3f right = (inv_view * Vector4f(1.0f, 0.0f, 0.0f, 0.0f)).topRows(3);
    const Vector3f up    = (inv_view * Vector4f(0.0f, 1.0f, 0.0f, 0.0f)).topRows(3);
    const ImVec2 pos     = io.MousePos;
    const ImVec2 mouse_delta(pos.x - previous_pos.x, pos.y - previous_pos.y);
    const float coeff =
        mouse_translation_factor * (main_camera->position - main_camera->target).norm();
    const Vector3f delta = coeff * (-mouse_delta.x * right + mouse_delta.y * up);
    main_camera->target += delta;
    main_camera->position += delta;
    previous_pos = pos;
}
