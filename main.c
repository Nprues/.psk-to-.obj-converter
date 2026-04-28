#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <windows.h>

// -------------------- PSK CHUNK HEADER --------------------

typedef struct {
    char id[20];
    int32_t type;
    int32_t data_size;
    int32_t count;
} ChunkHeader;

// -------------------- DATA STRUCTS --------------------

typedef struct {
    float x, y, z;
} Point;

typedef struct {
    uint16_t point_index;
    float u;
    float v;
    uint8_t material;
    uint8_t reserved;
} Wedge;

typedef struct {
    uint16_t v0, v1, v2;
    uint8_t material;
    uint8_t aux;
    int32_t smoothing;
} Face;

// -------------------- UTIL --------------------

static int read_bytes(FILE* f, void* dst, size_t size) {
    return fread(dst, 1, size, f) == size;
}

static void skip_chunk(FILE* f, int32_t data_size, int32_t count) {
    if (data_size > 0 && count > 0)
        fseek(f, (long)data_size * count, SEEK_CUR);
}

static void build_output_path(const char* input, char* output, size_t sz) {
    const char* last_slash = strrchr(input, '/');
    const char* last_back = strrchr(input, '\\');
    const char* last = last_slash > last_back ? last_slash : last_back;
    const char* file = last ? last + 1 : input;

    char base[512];
    strncpy(base, file, sizeof(base) - 1);
    base[sizeof(base) - 1] = '\0';

    char* dot = strrchr(base, '.');
    if (dot) *dot = '\0';

    snprintf(output, sz, "%s.obj", base);
}

// -------------------- CORE --------------------

void process_file(const char* input) {

    FILE* f = fopen(input, "rb");
    if (!f) {
        printf("Error abriendo %s: %s\n", input, strerror(errno));
        return;
    }

    char output[512];
    build_output_path(input, output, sizeof(output));

    printf("\nInput : %s\n", input);
    printf("Output: %s\n", output);

    ChunkHeader header;
    if (!read_bytes(f, &header, sizeof(header))) {
        printf("Error leyendo header\n");
        fclose(f);
        return;
    }

    if (strncmp(header.id, "ACTRHEAD", 8) != 0) {
        printf("No es PSK valido\n");
        fclose(f);
        return;
    }

    Point* points = NULL;
    Wedge* wedges = NULL;
    Face* faces = NULL;

    int point_count = 0;
    int wedge_count = 0;
    int face_count = 0;

    while (1) {

        ChunkHeader c;
        if (!read_bytes(f, &c, sizeof(c)))
            break;

        if (strncmp(c.id, "PNTS0000", 8) == 0) {

            point_count = c.count;
            points = malloc(sizeof(Point) * point_count);

            if (!read_bytes(f, points, sizeof(Point) * point_count)) {
                printf("Error PNTS\n");
                goto cleanup;
            }
        }

        else if (strncmp(c.id, "VTXW0000", 8) == 0) {

            wedge_count = c.count;
            wedges = malloc(sizeof(Wedge) * wedge_count);

            for (int i = 0; i < wedge_count; i++) {
                unsigned char b[16];
                if (!read_bytes(f, b, 16)) goto cleanup;

                wedges[i].point_index = *(uint16_t*)(b + 0);
                wedges[i].u = *(float*)(b + 2);
                wedges[i].v = *(float*)(b + 6);
                wedges[i].material = b[10];
                wedges[i].reserved = b[11];
            }
        }

        else if (strncmp(c.id, "FACE0000", 8) == 0 ||
                 strncmp(c.id, "FACE3200", 8) == 0) {

            face_count = c.count;
            faces = malloc(sizeof(Face) * face_count);

            if (!read_bytes(f, faces, sizeof(Face) * face_count)) {
                printf("Error FACE\n");
                goto cleanup;
            }
        }

        else {
            skip_chunk(f, c.data_size, c.count);
        }
    }

    if (!points || !wedges || !faces) {
        printf("PSK incompleto\n");
        goto cleanup;
    }

    FILE* out = fopen(output, "w");
    if (!out) goto cleanup;

    fprintf(out, "# PSK converted\n");

    for (int i = 0; i < wedge_count; i++) {
        Point p = points[wedges[i].point_index];

        fprintf(out, "v %f %f %f\n", p.x, p.z, p.y);
    }

    for (int i = 0; i < wedge_count; i++)
        fprintf(out, "vt %f %f\n", wedges[i].u, 1.0f - wedges[i].v);

    for (int i = 0; i < face_count; i++)
        fprintf(out, "f %d/%d %d/%d %d/%d\n",
                faces[i].v0 + 1, faces[i].v0 + 1,
                faces[i].v1 + 1, faces[i].v1 + 1,
                faces[i].v2 + 1, faces[i].v2 + 1);

    fclose(out);
    printf("OK\n");

cleanup:
    fclose(f);
    free(points);
    free(wedges);
    free(faces);
}

// -------------------- MAIN --------------------

int main() {

    WIN32_FIND_DATA fd;
    HANDLE h;

    // Buscar .psk
    h = FindFirstFile("*.psk", &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            process_file(fd.cFileName);
        } while (FindNextFile(h, &fd));
        FindClose(h);
    }

    // Buscar .pskx
    h = FindFirstFile("*.pskx", &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            process_file(fd.cFileName);
        } while (FindNextFile(h, &fd));
        FindClose(h);
    }

    printf("\nTerminado.\n");
    return 0;
}