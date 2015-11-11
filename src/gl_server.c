/* Copyright 2013 Ka-Ping Yee

Licensed under the Apache License, Version 2.0 (the "License"); you may not
use this file except in compliance with the License.  You may obtain a copy
of the License at: http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software distributed
under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
CONDITIONS OF ANY KIND, either express or implied.  See the License for the
specific language governing permissions and limitations under the License. */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>

#ifdef __APPLE__
#include <OpenGL/CGLCurrent.h>
#include <OpenGL/CGLTypes.h>
#include <OpenGL/OpenGL.h>
#include <GLUT/glut.h>
#else
#include <GL/glut.h>
#endif

#include "cJSON.h"
#include "opc.h"

opc_source source = -1;
int verbose = 0;
int bUseJSONColor = 0; // Option set by 'jsoncolor' on command line... uses color in the JSON file rather than color from the client

// Camera parameters
#define FOV_DEGREES 20
#define WINDOW_WIDTH 800
int orbiting = 0, dollying = 0;
double start_angle, start_elevation, start_distance;
int start_x, start_y;
double orbit_angle = 192.0;  // camera orbit angle, degrees
double camera_elevation = -15;  // camera elevation angle, degrees
double camera_distance = 38.0;  // distance from origin, metres
double camera_aspect = 1.0;  // will be updated to match window aspect ratio

// Add panning, at least in world coordinates. This is a bit of a hack from a
// UI perspective, but it works.  Arrow keys pan along the X and Y axis, PageUp
// and PageDown pan along the Z axis.
double world_x = 0.0; 
double world_y = 0.0;
double world_z = 0.0;

// Shape parameters
#define SHAPE_THICKNESS 0.18  // thickness of points and lines, metres

// LED colours
#define MAX_PIXELS 30000
int num_pixels = 0;
pixel pixels[MAX_PIXELS];

// Floating-point colours
typedef struct {
  double r, g, b;
} colour;

colour tmp_colour;
#define set_colour(c) ((tmp_colour = c), glColor3dv(&(tmp_colour.r)))
#define set_rgb(r, g, b) (glColor3d(r, g, b))
colour xfer[256];

// Vector arithmetic
typedef struct {
  double x, y, z;
} vector;

// Soma's button state.  See the press() function, below.
double button_timeout[2];
char *button_filename[2] = {
  "/tmp/buttonA",
  "/tmp/buttonB",
};

vector tmp_vector;
#define put_vertex(v) ((tmp_vector = v), glVertex3dv(&(tmp_vector.x)))
#define put_pair(v, w) (put_vertex(v), put_vertex(w))

vector add(vector v, vector w) {
  vector result;
  result.x = v.x + w.x;
  result.y = v.y + w.y;
  result.z = v.z + w.z;
  return result;
}

vector subtract(vector v, vector w) {
  vector result;
  result.x = v.x - w.x;
  result.y = v.y - w.y;
  result.z = v.z - w.z;
  return result;
}

vector multiply(double f, vector v) {
  vector result;
  result.x = f*v.x;
  result.y = f*v.y;
  result.z = f*v.z;
  return result;
}

double length(vector v) {
  return sqrt(v.x*v.x + v.y*v.y + v.z*v.z);
}

double dot(vector v, vector w) {
  return v.x*w.x + v.y*w.y + v.z*w.z;
}

vector cross(vector v, vector w) {
  vector result;
  result.x = v.y*w.z - w.y*v.z;
  result.y = v.z*w.x - w.z*v.x;
  result.z = v.x*w.y - w.x*v.y;
  return result;
}

// Shapes
typedef struct shape {
  void (*draw)(struct shape* this, GLUquadric* quad);
  int index;
  double red;
  double green;
  double blue;
  union {
    vector point;
    struct { vector start, end; } line;
  } g;
} shape;

#define MAX_SHAPES 30000
int num_shapes = 0;
shape shapes[MAX_SHAPES];

void draw_point(shape* this, GLUquadric* quad) {
  pixel p = pixels[this->index];
  if (bUseJSONColor) {
    glColor3d(this->red, this->green, this->blue);
  } else {
    glColor3d(xfer[p.r].r, xfer[p.g].g, xfer[p.b].b);
  }
  glPushMatrix();
  glTranslatef(this->g.point.x, this->g.point.y, this->g.point.z);
  gluSphere(quad, SHAPE_THICKNESS/2, 6, 3);
  glPopMatrix();
}

void draw_line(shape* this, GLUquadric* quad) {
  pixel p = pixels[this->index];
  vector start = this->g.line.start;
  vector delta = subtract(this->g.line.end, this->g.line.start);
  vector z = {0, 0, 1};
  vector hinge = cross(z, delta);
  double len = length(delta);
  double angle = 180./M_PI * acos(dot(z, delta) / len);
  glColor3d(xfer[p.r].r, xfer[p.g].g, xfer[p.b].b);
  glPushMatrix();
  glTranslated(start.x, start.y, start.z);
  glRotated(angle, hinge.x, hinge.y, hinge.z);
  gluSphere(quad, SHAPE_THICKNESS/2, 6, 3);
  gluCylinder(quad, SHAPE_THICKNESS/2, SHAPE_THICKNESS/2, len, 6, 1);
  glTranslated(0, 0, len);
  gluSphere(quad, SHAPE_THICKNESS/2, 6, 3);
  glPopMatrix();
}

void draw_axes() {
  vector o = {0, 0, 0};
  vector x = {1, 0, 0};
  vector y = {0, 1, 0};
  vector z = {0, 0, 1};
  vector xx = {10, 0, 0};
  vector yy = {0, 10, 0};
  vector zz = {0, 0, 10};
  glLineWidth(2);
  glBegin(GL_LINES);
  set_rgb(0.3, 0.3, 0.3);
  put_pair(o, x);
  put_pair(o, y);
  put_pair(o, z);
  set_rgb(0.3, 0, 0);
  put_pair(x, xx);
  set_rgb(0, 0.3, 0);
  put_pair(y, yy);
  set_rgb(0, 0, 0.3);
  put_pair(z, zz);
  glEnd();
}

// Mesh loaded from STL file, if any
float * mesh = NULL; // array of triangle vertices
int num_triangles = 0;

char* read_file(char* filename);

void load_stl_mesh(char* filename) {

  // check for valid file extension
  char* ext = strrchr(filename,'.');
  if (!ext || (strcmp(ext+1, "stl") && strcmp(ext+1, "STL")) || strlen(ext+1) > 3) {
    printf("Invalid filename: %s. Mesh file must be an STL file.\n", filename);
    return;
  }

  char* data = read_file(filename);
  if (!data) {
    printf("Invalid filename: %s. File does not exist or can't be read.\n", filename);
    return;
  }

  int float_size = sizeof(float);
  if (float_size == 4)
  {
    char* file_loc = data;

    // STL has 80-byte header followed by uint32 telling you the # of triangles in the file
    file_loc += 80;
    num_triangles = *(u32*)(file_loc);

    file_loc += 4; //skip triangle count
    file_loc += 3*float_size; //skip normal of first triangle
    
    mesh = (float*)malloc(num_triangles*9*float_size);
    if (!mesh) {
      printf("Could not allocate memory for STL mesh data.\n");
      num_triangles = 0;
    }
    else {
      float* loc = mesh;
      int i;

      // each STL triangle is 3 normal floats, then 9 vertex floats, then a uint16.
      // skipping the normals for now since we don't have a lighting model that 
      // requires them
      for (i = 0; i < num_triangles; i++) {
        memcpy(loc, file_loc, 9*float_size);
        loc += 9;
        file_loc += 12*float_size + 2;
      }
    }
  }
  free(data);
}

void render_triangles(float* triangles, int size)
{
  if (triangles) {
    glBegin(GL_TRIANGLES);

    // render everything in grey (for now)
    glColor3d(0.4, 0.4, 0.4);

    int i;
    for(i = 0; i<size; i+=3)
      glVertex3fv(triangles+i);

    glEnd();
  }
}

void display() {
  int i;
  shape* sh;

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  draw_axes();
  GLUquadric* quad = gluNewQuadric();
  for (i = 0, sh = shapes; i < num_shapes; i++, sh++) {
    sh->draw(sh, quad);
  }
  render_triangles(mesh, num_triangles*9);
  gluDeleteQuadric(quad);
  glutSwapBuffers();
}

void update_camera() {
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  gluPerspective(FOV_DEGREES, camera_aspect, 0.1, 1e3); // fov, aspect, zrange
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  double camera_y = -cos(camera_elevation*M_PI/180)*camera_distance;
  double camera_z = sin(camera_elevation*M_PI/180)*camera_distance;
  gluLookAt(0, camera_y, camera_z, /* target */ 0, 0, 0, /* up */ 0, 0, 1);
  glRotatef(orbit_angle, 0, 0, 1);
  glTranslatef(world_x, world_y, world_z);
  display();
}

void reshape(int width, int height) {
  glViewport(0, 0, width, height);
  camera_aspect = ((double) width)/((double) height);
  update_camera();
}

void mouse(int button, int state, int x, int y) {
  if (state == GLUT_DOWN && glutGetModifiers() & GLUT_ACTIVE_SHIFT) {
    dollying = 1;
    start_distance = camera_distance;
    start_x = x;
    start_y = y;
  } else if (state == GLUT_DOWN) {
    orbiting = 1;
    start_angle = orbit_angle;
    start_elevation = camera_elevation;
    start_x = x;
    start_y = y;
  } else {
    orbiting = 0;
    dollying = 0;
  }
}

void motion(int x, int y) {
  if (orbiting) {
    orbit_angle = start_angle + (x - start_x)*1.0;
    double elevation = start_elevation + (y - start_y)*1.0;
    camera_elevation = elevation < -89 ? -89 : elevation > 89 ? 89 : elevation;
    update_camera();
  }
  if (dollying) {
    double distance = start_distance + (y - start_y)*0.1;
    camera_distance = distance < 1.0 ? 1.0 : distance;
    update_camera();
  }
}

/*
 * Returns unix time as a floating point number
 */
double now(void) {
  struct timeval tv;

  memset(&tv, 0, sizeof(tv));
  gettimeofday(&tv, NULL);
  return tv.tv_sec + tv.tv_usec/1000000.0;
}

/*
 * Very specific to FLG's Soma sculpture.  If "0" is pressed, simulate pressing
 * the left button, and "1" for the right.  This only works if "/var/run/soma"
 * writable.
 */
void press(int num) {
  int fd;

  fd = creat(button_filename[num], 0666);

  if (fd < 0) {
    printf("Failed to touch %s. Is directory writable?\n", button_filename[num]);
  }

  else {
    if (button_timeout[num] == 0) {
      printf("Pressing  %s\n", button_filename[num]);
    }
    button_timeout[num] = now() + 0.25;
  }

  close(fd);
}

void keyboard(unsigned char key, int x, int y) {
  switch(key) {
    case 'q':
    case '\x1b':
      exit(0);

    case 'x':  world_x++;  break;
    case 'X':  world_x--;  break;
    case 'y':  world_y++;  break;
    case 'Y':  world_y--;  break;
    case 'z':  world_z++;  break;
    case 'Z':  world_z--;  break;

    case '1':
    case 'a':
    case 'l':
      press(0);  break;

    case '0':
    case 'b':
    case 'r':
      press(1);  break;
  }

  update_camera();
}

void special_keyboard(int key, int x, int y) {
  switch (key) {
    case GLUT_KEY_UP:         keyboard('x', 0, 0);  break;
    case GLUT_KEY_DOWN:       keyboard('X', 0, 0);  break;
    case GLUT_KEY_RIGHT:      keyboard('y', 0, 0);  break;
    case GLUT_KEY_LEFT:       keyboard('Y', 0, 0);  break;
    case GLUT_KEY_PAGE_UP:    keyboard('z', 0, 0);  break;
    case GLUT_KEY_PAGE_DOWN:  keyboard('Z', 0, 0);  break;
  }
}

void handler(u8 channel, u16 count, pixel* p) {
  int i = 0;

  for (i = 0; i < 2; i++) {
    if (button_timeout[i] && button_timeout[i] < now()) {
      printf("Releasing %s\n\n", button_filename[i]);
      unlink(button_filename[i]);
      button_timeout[i] = 0;
    }
  }

  if (verbose) {
    char* sep = " =";
    printf("-> channel %d: %d pixel%s", channel, count, count == 1 ? "" : "s");
    for (i = 0; i < count; i++) {
      if (i >= 4) {
        printf(", ...");
        break;
      }
      printf("%s %02x %02x %02x", sep, p[i].r, p[i].g, p[i].b);
      sep = ",";
    }
    printf("\n");
  }

  for (i = 0; i < count; i++) {
    pixels[i] = p[i];
  }
}

void idle() {
  /*
   * Receive all pending frames. We'll often draw slower than an OPC source
   * is producing pixels; to avoid runaway lag due to data buffered in the socket,
   * we want to skip frames.
   *
   * A short timeout (20 ms) on the first receive keeps us responsive to mouse events.
   * A zero timeout on subsequent receives lets us drain any queued frames without
   * waiting for them.
   */

  if (opc_receive(source, handler, 20) > 0) {

    // Drain queue
    while (opc_receive(source, handler, 0) > 0);

    // Show the last received frame
    display();
  }
}

char* read_file(char* filename) {
  FILE* fp;
  struct stat st;
  char* buffer;

  if (stat(filename, &st) != 0) {
    return strdup("");
  }
  buffer = malloc(st.st_size + 1);
  fp = fopen(filename, "r");
  fread(buffer, st.st_size, 1, fp);
  fclose(fp);
  buffer[st.st_size] = 0;
  return buffer;
}


// Read in the JSON file and ... do something to it.
void init(char* filename) {
  char* buffer;
  cJSON* json;
  cJSON* item;
  cJSON* index;
  cJSON* point;
  cJSON* color;
  cJSON* x;
  cJSON* line;
  cJSON* start;
  cJSON* x2;
  int i = 0;
  int isValidShape = 0;
  
  buffer = read_file(filename);
  json = cJSON_Parse(buffer);
  free(buffer);

  num_shapes = 0;
  for (item = json->child, i = 0; item; item = item->next, i++) {
    // Reset the index information if necessary
    index = cJSON_GetObjectItem(item, "index");
    if (index) {
        printf("Resetting index to %d\n", index->valueint);
      i = index->valueint;
    }
    // Extract point information, if it exists...
    point = cJSON_GetObjectItem(item, "point");
    x = point ? point->child : NULL;
    if (x && x->next && x->next->next) {
      shapes[num_shapes].draw = draw_point;
      shapes[num_shapes].index = i;
      shapes[num_shapes].g.point.x = x->valuedouble;
      shapes[num_shapes].g.point.y = x->next->valuedouble;
      shapes[num_shapes].g.point.z = x->next->next->valuedouble;
      isValidShape = 1;
    }

    // Extract line information, if we haven't already figured out what kind of shape
    // this is (a shape can be a point or a line, but not both)
    if (!isValidShape) {
        line = cJSON_GetObjectItem(item, "line");
        start = line ? line->child : NULL;
        x = start ? start->child : NULL;
        x2 = start && start->next ? start->next->child : NULL;
        if (x && x->next && x->next->next && x2 && x2->next && x2->next->next) {
          shapes[num_shapes].draw = draw_line;
          shapes[num_shapes].index = i;
          shapes[num_shapes].g.line.start.x = x->valuedouble;
          shapes[num_shapes].g.line.start.y = x->next->valuedouble;
          shapes[num_shapes].g.line.start.z = x->next->next->valuedouble;
          shapes[num_shapes].g.line.end.x = x2->valuedouble;
          shapes[num_shapes].g.line.end.y = x2->next->valuedouble;
          shapes[num_shapes].g.line.end.z = x2->next->next->valuedouble;
        }
    }

    // If we have a valid shape, extract color information and update index
    // into shape table.
    if (isValidShape) {
        color = cJSON_GetObjectItem(item, "color");
        x = color ? color->child : NULL;
        if (x && x->next &&  x->next->next) {
            shapes[num_shapes].red = x->valuedouble;
            shapes[num_shapes].green = x->next->valuedouble;
            shapes[num_shapes].blue = x->next->next->valuedouble;
        } else {
            shapes[num_shapes].red = 1.0;
            shapes[num_shapes].green = 1.0;
            shapes[num_shapes].blue = 1.0;
        }
        num_shapes++;
    }
  }
  
  // nb - the following clause does nothing, and we're relying
  // on the compiler to initialize the pixels array.
  // This is probably a bad idea....
  num_pixels = i;
  for (i = 0; i < num_pixels; i++) {
    pixels[i].r = pixels[i].g = pixels[i].b = 1;
  }
  
  // Initialize the channel transformation vectors. The pixel information
  // comes to us as a value between 0 and 255, and we need to transform 
  // that to something between 0 and 1.0f
  for (i = 0; i < 256; i++) {
    xfer[i].r = xfer[i].g = xfer[i].b = 0.1 + i*0.9/256;
  }
}

int main(int argc, char** argv) {
  u16 port;
  int i;

  glutInitWindowSize(WINDOW_WIDTH, WINDOW_WIDTH*0.75);
  glutInit(&argc, argv);
  if (argc < 2) {
    fprintf(stderr, "Usage: %s <gl-options> <filename.json> [<port>] [meshfile.stl]\n", argv[0]);
    exit(1);
  }

  // Start with the buttons unpressed
  for (i = 0; i < 2; i++) {
    unlink(button_filename[i]);
  }

  init(argv[1]);
  port = argc > 2 ? strtol(argv[2], NULL, 10) : 0;
  port = port ? port : OPC_DEFAULT_PORT;
  source = opc_new_source(port);

  if (argc > 3) {
    load_stl_mesh(argv[3]);
  }
  
  // currently undocumented option to take shape color values from the initial json file.
  // Handy for some types of debugging.
  if (argc > 4){
    if (!strcasecmp("jsoncolor", argv[4])) {
        bUseJSONColor = 1;
    }
  } 

  glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
  glutCreateWindow("OPC");
  glutReshapeFunc(reshape);
  glutDisplayFunc(display);
  glutMouseFunc(mouse);
  glutMotionFunc(motion);
  glutIgnoreKeyRepeat(1);
  glutKeyboardFunc(keyboard);        // https://www.opengl.org/documentation/specs/glut/spec3/node49.html
  glutSpecialFunc(special_keyboard); // https://www.opengl.org/resources/libraries/glut/spec3/node54.html
  glutIdleFunc(idle);

  glEnable(GL_DEPTH_TEST);
#ifdef __APPLE__
  /* Make glutSwapBuffers wait for vertical refresh to avoid frame tearing. */
  int swap_interval = 1;
  CGLContextObj context = CGLGetCurrentContext();
  CGLSetParameter(context, kCGLCPSwapInterval, &swap_interval);
#endif

  glutMainLoop();
  return 0;
}

// vim:set ts=2 sw=2 ai et:
