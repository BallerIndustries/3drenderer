#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <SDL2/SDL.h>
#include "display.h"
#include "vector.h"
#include "mesh.h"
#include "array.h"
#include "matrix.h"
 
triangle_t* triangles_to_render = NULL;
vec3_t camera_position = { .x = 0, .y = 0, .z = 0 };

float fov_factor = 640;

bool is_running = false;
int previous_frame_time = 0;

SDL_Window* window = NULL;
SDL_Renderer* renderer = NULL;
uint32_t* color_buffer = NULL;
SDL_Texture* color_buffer_texture = NULL;
int window_width = 800;
int window_height = 600;

//int display_mode = 1;
bool cull_back_faces = true;
bool draw_vertex_dot = true;
bool draw_wireframes = true;
bool draw_filled_triangles = false;


void setup(void) {
    // Allocate the required memory in bytes to hold the color buffer
    color_buffer = (uint32_t*) malloc(sizeof(uint32_t) * window_width * window_height);

    // Creating a SDL texture that is used to display the color buffer
    color_buffer_texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING,
        window_width,
        window_height
    );

    load_cube_mesh_data();
}

void handle_keypress(int key) {
    switch (key) {
        case SDLK_ESCAPE:
            is_running = false;
            break;

        case SDLK_1:
            draw_vertex_dot = true;
            draw_wireframes = true;
            draw_filled_triangles = false;
            break;

        case SDLK_2:
            draw_vertex_dot = false;
            draw_wireframes = true;
            draw_filled_triangles = false;
            break;

        case SDLK_3:
            draw_vertex_dot = false;
            draw_wireframes = false;
            draw_filled_triangles = true;
            break;

        case SDLK_4:
            draw_vertex_dot = false;
            draw_wireframes = true;
            draw_filled_triangles = true;
            break;

        case SDLK_c:
            cull_back_faces = true;
            break;

        case SDLK_d:
            cull_back_faces = false;
            break;
    }
}

void process_input(void) {
    SDL_Event event;
    SDL_PollEvent(&event);

    switch (event.type) {
        case SDL_QUIT:
            is_running = false;
            break;

        case SDL_KEYDOWN:
            handle_keypress(event.key.keysym.sym);
            break;
    }
}

// Functions that received a 3D vector and returns a projected 2D point
vec2_t project(vec3_t point) {
    vec2_t projected_point = {
        .x = (fov_factor * point.x) / point.z,
        .y = (fov_factor * point.y) / point.z
    };

    return projected_point;
}

void update(void) {
    while (!SDL_TICKS_PASSED(SDL_GetTicks(), previous_frame_time + FRAME_TARGET_TIME));

    previous_frame_time = SDL_GetTicks();

    triangles_to_render = NULL;

    // Change the scale/rotation values per animation frame
    mesh.rotation.x += 0.01;
    mesh.rotation.y += 0.01;
    mesh.rotation.z += 0.01;

    mesh.translation.x += 0.01;
    mesh.translation.z = 5.0;

    // Create a scale, rotation and translation matrices that will be used to mulitply the mesh vertices
    mat4_t scale_matrix = mat4_make_scale(mesh.scale.x, mesh.scale.y, mesh.scale.y);
    mat4_t translation_matrix = mat4_make_translation(mesh.translation.x, mesh.translation.y, mesh.translation.z);
    mat4_t rotation_matrix_x = mat4_make_rotation_x(mesh.rotation.x);
    mat4_t rotation_matrix_y = mat4_make_rotation_y(mesh.rotation.y);
    mat4_t rotation_matrix_z = mat4_make_rotation_z(mesh.rotation.z);

    int num_faces = array_length(mesh.faces); 

    for (int i = 0; i < num_faces; i++) {
        face_t mesh_face = mesh.faces[i];

        vec3_t face_vertices[3];
        face_vertices[0] = mesh.vertices[mesh_face.a - 1];
        face_vertices[1] = mesh.vertices[mesh_face.b - 1];
        face_vertices[2] = mesh.vertices[mesh_face.c - 1];

        vec4_t transformed_vertices[3];

        // Loop all three vertices of this current face and apply transformations
        for (int j = 0; j < 3; j++) {
            vec4_t transformed_vertex = vec4_from_vec3(face_vertices[j]);

            // Create a world matrix combining scale, rotation and translation matrices
            mat4_t world_matrix = mat4_identity(); 
            world_matrix = mat4_mul_mat4(scale_matrix, world_matrix);
            world_matrix = mat4_mul_mat4(rotation_matrix_x, world_matrix);
            world_matrix = mat4_mul_mat4(rotation_matrix_y, world_matrix);
            world_matrix = mat4_mul_mat4(rotation_matrix_z, world_matrix);
            world_matrix = mat4_mul_mat4(translation_matrix, world_matrix);

            // Multiply the world matrix by the original vector
            transformed_vertex = mat4_mul_vec4(world_matrix, transformed_vertex);

            // Save transformed vertex
            transformed_vertices[j] = transformed_vertex;
        }

        // TODO: Check backface culling
        if (cull_back_faces) {
            vec3_t vector_a = vec3_from_vec4(transformed_vertices[0]); /*    A    */
            vec3_t vector_b = vec3_from_vec4(transformed_vertices[1]); /*   / \   */
            vec3_t vector_c = vec3_from_vec4(transformed_vertices[2]); /*  C---B  */

            // Get the vector subtraction of B-A and C-A
            vec3_t vector_ab = vec3_sub(vector_b, vector_a);
            vec3_t vector_ac = vec3_sub(vector_c, vector_a);

            // Compute the face normal (using the cross product to find perpendicular)
            vec3_t normal =  vec3_cross(vector_ab, vector_ac);

            // Normalize the face normal vector
            vec3_normalize(&normal);

            // Find the vector between a point in the triangle and the camera origin
            vec3_t camera_ray = vec3_sub(camera_position, vector_a);

            // Calculate how aligned the camera ray is with the face normal (using dot product)
            float dot_normal_camera = vec3_dot(normal, camera_ray);
            //printf("dot_normal_camera = %f\n", dot_normal_camera);

            if (dot_normal_camera < 0) {
                continue;
            }
        }

        vec2_t projected_points[3];

        // Loop all three vertices to perform projection
        for (int j = 0; j < 3; j++) {
            // Project the current vertex
            projected_points[j] = project(vec3_from_vec4(transformed_vertices[j]));

            // Scale and translate the projected points to the middle of the screen
            projected_points[j].x += (window_width / 2);
            projected_points[j].y += (window_height / 2);
        }

        float avg_depth = (transformed_vertices[0].z + transformed_vertices[1].z + transformed_vertices[2].z) / 3.0;

        triangle_t projected_triangle = {
            .points = {
                { projected_points[0].x, projected_points[0].y, },
                { projected_points[1].x, projected_points[1].y, },
                { projected_points[2].x, projected_points[2].y, },
            },
            .color = mesh_face.color,
            .avg_depth = avg_depth
        };

        array_push(triangles_to_render, projected_triangle);
    }

    // TODO: Sort the triangles to render by their average depth
    int num_triangles = array_length(triangles_to_render);

    for (int i = 0; i < num_triangles; i++) {
        for (int j = i; j < num_triangles; j++) {
            if (triangles_to_render[i].avg_depth < triangles_to_render[j].avg_depth) {
                triangle_t temp = triangles_to_render[i];
                triangles_to_render[i] = triangles_to_render[j];
                triangles_to_render[j] = temp;
            }
        }
    }
}

void render(void) {
    draw_grid();

    int num_triangles = array_length(triangles_to_render);

    for (int i = 0; i < num_triangles; i++) {
        triangle_t triangle = triangles_to_render[i];

        if (draw_vertex_dot) {
            draw_rectangle(triangle.points[0].x, triangle.points[0].y, 5, 5, 0xFFFF0000);
            draw_rectangle(triangle.points[1].x, triangle.points[1].y, 5, 5, 0xFFFF0000);
            draw_rectangle(triangle.points[2].x, triangle.points[2].y, 5, 5, 0xFFFF0000);
        }
        if (draw_filled_triangles) {
            draw_filled_triangle(
                triangle.points[0].x, triangle.points[0].y,
                triangle.points[1].x, triangle.points[1].y,
                triangle.points[2].x, triangle.points[2].y,
                triangle.color
            );
        }
        if (draw_wireframes) {
            draw_triangle(
                triangle.points[0].x, triangle.points[0].y,
                triangle.points[1].x, triangle.points[1].y,
                triangle.points[2].x, triangle.points[2].y,
                0xFF00FF00
            );
        }
    }
    
    render_color_buffer();
    clear_color_buffer(0xFF000000);
    SDL_RenderPresent(renderer);
}

void free_resources() {
    free(color_buffer);
    array_free(mesh.faces);
    array_free(mesh.vertices);
    array_free(triangles_to_render);
    
}

int main(void) {
    is_running = initialize_window();

    setup();

    while (is_running) {
        process_input();
        update();
        render();
    }

    destroy_window();

    return 0;
}
