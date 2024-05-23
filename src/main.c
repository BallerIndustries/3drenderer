#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <SDL2/SDL.h>
#include "display.h"
#include "vector.h"
#include "mesh.h"
#include "array.h"
#include "matrix.h"
#include "light.h"
#include "texture.h"
#include "triangle.h"
 
triangle_t* triangles_to_render = NULL;
vec3_t camera_position = { .x = 0, .y = 0, .z = 0 };
light_t light = { .direction = { .x = 0.0, .y = 0.0, .z = -1.0 }};
mat4_t proj_matrix;

bool is_running = false;
int previous_frame_time = 0;

SDL_Window* window = NULL;
SDL_Renderer* renderer = NULL;
uint32_t* color_buffer = NULL;
SDL_Texture* color_buffer_texture = NULL;
int window_width = 800;
int window_height = 600;

enum cull_method cull_method;
enum render_method render_method;

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

    // Initialise the perspective projection matrix
    float fov = M_PI / 3;
    float aspect = (float)window_height / (float)window_width;
    float znear = 0.1;
    float zfar = 100.0;
    proj_matrix = mat4_make_perspective(fov, aspect, znear, zfar);
    
    // Manually load the hardcoded texture date from the static array
    mesh_texture = (uint32_t*)REDBRICK_TEXTURE;
    texture_width = 64;
    texture_height = 64;

    load_cube_mesh_data();
    // load_obj_file_data();
}

void handle_keypress(int key) {
    switch (key) {
        case SDLK_ESCAPE:
            is_running = false;
            break;

        case SDLK_1:
            render_method = RENDER_WIRE;
            break;

        case SDLK_2:
            render_method = RENDER_WIRE_VERTEX;
            break;

        case SDLK_3:
            render_method = RENDER_FILL_TRIANGLE;
            break;

        case SDLK_4:
            render_method = RENDER_FILL_TRIANGLE_WIRE;
            break;

        case SDLK_5:
            render_method = RENDER_TEXTURED;
            break;
            
        case SDLK_6:
            render_method = RENDER_TEXTURED_WIRE;
            break;

        case SDLK_c:
            cull_method = CULL_NONE;
            break;

        case SDLK_d:
            cull_method = CULL_BACKFACE;
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

void update(void) {
    while (!SDL_TICKS_PASSED(SDL_GetTicks(), previous_frame_time + FRAME_TARGET_TIME));

    previous_frame_time = SDL_GetTicks();

    triangles_to_render = NULL;

    // Change the scale/rotation values per animation frame
    // mesh.rotation.x += 0.01;
    // mesh.rotation.y += 0.01;
    // mesh.rotation.z += 0.01;

    //mesh.translation.x += 0.01;
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
        float dot_normal_light_source = vec3_dot(normal, light.direction);

        if (cull_method == CULL_BACKFACE && dot_normal_camera < 0) {
            continue;
        }

        vec4_t projected_points[3];

        // Loop all three vertices to perform projection
        for (int j = 0; j < 3; j++) {
            // Project the current vertex
            projected_points[j] = mat4_mul_vec4_project(proj_matrix, transformed_vertices[j]);

            // Scale into the view port
            projected_points[j].x *= (window_width / 2.0);
            projected_points[j].y *= (window_height / 2.0);

            // Invert the y values to account for flipped screen y cooridnate
            projected_points[j].y *= -1;

            // Translate the projected points to the middle of the screen
            projected_points[j].x += (window_width / 2.0);
            projected_points[j].y += (window_height / 2.0);   
        }

        float avg_depth = (transformed_vertices[0].z + transformed_vertices[1].z + transformed_vertices[2].z) / 3.0;
        uint32_t lighted_color = light_apply_intensity(mesh_face.color, dot_normal_light_source);

        triangle_t projected_triangle = {
            .points = {
                { projected_points[0].x, projected_points[0].y, projected_points[0].z, projected_points[0].w },
                { projected_points[1].x, projected_points[1].y, projected_points[1].z, projected_points[1].w },
                { projected_points[2].x, projected_points[2].y, projected_points[2].z, projected_points[2].w },
            },
            .texcoords = {
                { mesh_face.a_uv.u, mesh_face.a_uv.v },
                { mesh_face.b_uv.u, mesh_face.b_uv.v },
                { mesh_face.c_uv.u, mesh_face.c_uv.v }
            },
            .color = lighted_color,
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

        if (render_method == RENDER_WIRE) {
            draw_triangle(
                triangle.points[0].x, triangle.points[0].y,
                triangle.points[1].x, triangle.points[1].y,
                triangle.points[2].x, triangle.points[2].y,
                0xFF00FF00
            );
        }
        else if (render_method == RENDER_WIRE_VERTEX) {
            draw_triangle(
                triangle.points[0].x, triangle.points[0].y,
                triangle.points[1].x, triangle.points[1].y,
                triangle.points[2].x, triangle.points[2].y,
                0xFF00FF00
            );

            draw_rectangle(triangle.points[0].x, triangle.points[0].y, 5, 5, 0xFFFF0000);
            draw_rectangle(triangle.points[1].x, triangle.points[1].y, 5, 5, 0xFFFF0000);
            draw_rectangle(triangle.points[2].x, triangle.points[2].y, 5, 5, 0xFFFF0000);
        }
        else if (render_method == RENDER_FILL_TRIANGLE) {
            draw_filled_triangle(
                triangle.points[0].x, triangle.points[0].y,
                triangle.points[1].x, triangle.points[1].y,
                triangle.points[2].x, triangle.points[2].y,
                triangle.color
            );
        }
        else if (render_method == RENDER_FILL_TRIANGLE_WIRE) {
            draw_filled_triangle(
                triangle.points[0].x, triangle.points[0].y,
                triangle.points[1].x, triangle.points[1].y,
                triangle.points[2].x, triangle.points[2].y,
                triangle.color
            );
            
            draw_triangle(
                triangle.points[0].x, triangle.points[0].y,
                triangle.points[1].x, triangle.points[1].y,
                triangle.points[2].x, triangle.points[2].y,
                0xFF00FF00
            );
        }
        else if (render_method == RENDER_TEXTURED) {
            draw_textured_triangle(
                triangle.points[0].x, triangle.points[0].y, triangle.points[0].z, triangle.points[0].w, triangle.texcoords[0].u, triangle.texcoords[0].v,
                triangle.points[1].x, triangle.points[1].y, triangle.points[1].z, triangle.points[1].w, triangle.texcoords[1].u, triangle.texcoords[1].v,
                triangle.points[2].x, triangle.points[2].y, triangle.points[2].z, triangle.points[2].w, triangle.texcoords[2].u, triangle.texcoords[2].v, 
                mesh_texture
            );
        }
        else if (render_method == RENDER_TEXTURED_WIRE) {
            draw_textured_triangle(
                triangle.points[0].x, triangle.points[0].y, triangle.points[0].z, triangle.points[0].w, triangle.texcoords[0].u, triangle.texcoords[0].v,
                triangle.points[1].x, triangle.points[1].y, triangle.points[1].z, triangle.points[1].w, triangle.texcoords[1].u, triangle.texcoords[1].v,
                triangle.points[2].x, triangle.points[2].y, triangle.points[2].z, triangle.points[2].w, triangle.texcoords[2].u, triangle.texcoords[2].v, 
                mesh_texture
            );

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
