/********************************************************************
*
* tgx library example : displaying some nice meshes...
*
********************************************************************/

// This example runs on teensy 4.1 with ILI9341 via SPI. 
// the screen driver library : https://github.com/vindar/ILI9341_T4
#include <ILI9341Driver.h> 

// the tgx library 
#include <tgx.h> 

// let's not burden ourselves with the tgx:: prefix
using namespace tgx;

// font use to draw text on the screen. 
#include "ili9341_t3n_font_Arial.h"  

// meshes (stored in PROGMEM)
#include "3Dmodels/nanosuit/nanosuit.h"
#include "3Dmodels/elementalist/elementalist.h"
#include "3Dmodels/sinbad/sinbad.h"
#include "3Dmodels/cyborg/cyborg.h"
#include "3Dmodels/dennis/dennis.h"
#include "3Dmodels/manga3/manga3.h"
#include "3Dmodels/naruto/naruto.h"
#include "3Dmodels/R2D2/R2D2.h"
#include "3Dmodels/stormtrooper/stormtrooper.h"

// 30MHz SPI, we can go higher. 
#define SPI_SPEED		30000000

// pins (here on SPI1)
// !!! the ILI9341_T4 screen driver only works with hardware SPI and DC 
//     must be a valid cs pin for the corresponding SPI bus !!!
#define PIN_SCK			27
#define PIN_MISO		1
#define PIN_MOSI		26
#define PIN_DC			0
#define PIN_RESET		29
#define PIN_CS			30
#define PIN_BACKLIGHT   28  // set this to 255 if not connected to MCU. 
#define PIN_TOUCH_IRQ	32  // set this to  255 if not used (or not on the same spi bus)
#define PIN_TOUCH_CS	31  // set this to  255 if not used (or not on the same spi bus)

// the screen driver object
ILI9341_T4::ILI9341Driver tft(PIN_CS, PIN_DC, PIN_SCK, PIN_MOSI, PIN_MISO, PIN_RESET, PIN_TOUCH_CS, PIN_TOUCH_IRQ);

// 2 x 6K diff buffers (used by tft) for differential updates
ILI9341_T4::DiffBuffStatic<6000> diff1;
ILI9341_T4::DiffBuffStatic<6000> diff2;

// screen dimension (portrait mode)
static const int SLX = 240;
static const int SLY = 320;

// main screen framebuffer (150K in DTCM)
uint16_t fb[SLX * SLY];

// internal framebuffer (150K in DMAMEM) used by the ILI9431_T4 library for double buffering.
DMAMEM uint16_t internal_fb[SLX * SLY];

// zbuffer (300K in DMAMEM)
DMAMEM float zbuf[SLX * SLY];

// image that encapsulates fb.
Image<RGB565> im(fb, SLX, SLY);

// the 3D mesh drawer (with zbuffer, perspective projection, backface culling)
Renderer3D<RGB565, SLX, SLY, true, false> renderer;

// list of meshes to display
const Mesh3D<RGB565> * meshes[9] = { &nanosuit_1, &elementalist_1, &sinbad_1, &cyborg, &naruto_1, &manga3_1, &dennis, &R2D2, &stormtrooper };

// shaders to use
const int shader = TGX_SHADER_GOURAUD | TGX_SHADER_TEXTURE;


/**
* This function computes the object position
* according to the current time.
* Return true when it is time to change model.
**/
bool  moveModel() // remark: need to keep the tgx:: prefix in function signatures because arduino messes with ino files....
    {
    static elapsedMillis em = 0; // timer
    const float end1 = 6000;
    const float end2 = 2000;
    const float end3 = 6000;
    const float end4 = 2000;
    const float tot = end1 + end2 + end3 + end4;

    bool change = false;
    while (em > tot) { em -= tot; change = true; } // check if it is time to change the mesh. 

    float t = em; // current time
    const float dilat = 9; // scale model
    const float roty = 360 * (t / 4000); // rotate 1 turn every 4 seconds        
    float tz, ty;
    if (t < end1)
    { // far away
        tz = -25;
        ty = 0;
    }
    else
    {
        t -= end1;
        if (t < end2)
        { // zooming in
            t /= end2;
            tz = -25 + 15 * t;
            ty = -6 * t;
        }
        else
        {
            t -= end2;
            if (t < end3)
            { // close up
                tz = -10;
                ty = -6;
            }
            else
            { // zooming out
                t -= end3;
                t /= end4;
                tz = -10 - 15 * t;
                ty = -6 + 6 * t;
            }
        }
    }
    fMat4 M;
    M.setScale({ dilat, dilat, dilat }); // scale the model
    M.multRotate(-roty, { 0,1,0 }); // rotate around y
    M.multTranslate({ 0,ty, tz }); // translate
    renderer.setModelMatrix(M);
    return change;
    }



/**
* Overlay some info about the current mesh on the screen
**/
void drawInfo(tgx::Image<tgx::RGB565>& im, int shader, const tgx::Mesh3D<tgx::RGB565> * mesh)  // remark: need to keep the tgx:: prefix in function signatures because arduino messes with ino files....
{
    static elapsedMillis em = 0; // number of milli elapsed since last fps update
    static int fps = 0; // last fps 
    static int count = 0; // number of frame since the last update
    // recompute fps every second. 
    count++;
    if ((int)em > 1000)
    {
        em = 0;
        fps = count;
        count = 0;
    }
    // count the number of triangles in the mesh (by iterating over linked meshes)
    const Mesh3D<RGB565>* m = mesh;
    int nbt = 0;
    while (m != nullptr)
    {
        nbt += m->nb_faces;
        m = m->next;
    }
    // display some info 
    char buf[80];
    im.drawText((mesh->name != nullptr ? mesh->name : "[unnamed mesh]"), { 3,12 }, RGB565_Red, Arial_10);
    sprintf(buf, "%d triangles", nbt);
    im.drawText(buf, { 3,SLY - 21 }, RGB565_Red, Arial_10);
    sprintf(buf, "%s%s", (shader & TGX_SHADER_GOURAUD ? "Gouraud shading" : "flat shading"), (shader & TGX_SHADER_TEXTURE ? " / texturing" : ""));
    im.drawText(buf, { 3, SLY - 5 }, RGB565_Red, Arial_10);
    sprintf(buf, "%d FPS", fps);
    auto B = im.measureText(buf, { 0,0 }, Arial_10, false);
    im.drawText(buf, { SLX - B.lx() - 3,12 }, RGB565_Red, Arial_10);
}



void setup()
    {
    // initialize the ILI9341 screen
    while (!tft.begin(SPI_SPEED))
        {
        Serial.println("Initialization error...");
        delay(1000);
        }

    // ok. turn on backlight
    pinMode(PIN_BACKLIGHT, OUTPUT);
    digitalWrite(PIN_BACKLIGHT, HIGH);

    // setup the screen driver 
    tft.setRotation(0); // portrait mode
    tft.setFramebuffers(internal_fb); // double buffering
    tft.setDiffBuffers(&diff1, &diff2); // 2 diff buffers
    tft.setDiffGap(4); // small gap
    tft.setVSyncSpacing(0); // do not use v-sync because we want to measure the max framerate; 
    tft.setRefreshRate(140); // max refreshrate. 

    // setup the 3D renderer.
    renderer.setOffset(0, 0); //  image = viewport
    renderer.setImage(&im); // set the image to draw onto (ie the screen framebuffer)
    renderer.setZbuffer(zbuf, SLX * SLY); // set the z buffer for depth testing
    renderer.setPerspective(45, ((float)SLX) / SLY, 0.1f, 1000.0f);  // set the perspective projection matrix. 

    // if external ram is present, copy model textures to extram because it gives a few more fps. 
    #if defined(ARDUINO_TEENSY41)
    if (external_psram_size > 0)
        {
        for (auto & m : meshes)  m = copyMeshEXTMEM(m);
        }
    #endif

    }


// index of the mesh currently displayed
int meshindex = 0;

// number of frame drawn
int nbf = 0;


void loop()
{
    // erase the screen
    im.fillScreen(RGB565_Black);

    // clear the z buffer
    renderer.clearZbuffer();

    // move the model to it correct position (depending on current time). 
    if (moveModel())
        meshindex = (meshindex + 1) % (sizeof(meshes) / sizeof(meshes[0]));

    // draw the mesh on the image
    renderer.drawMesh(shader, meshes[meshindex]);

    // overlay some info 
    drawInfo(im, shader, meshes[meshindex]);

    // update the screen
    tft.update(fb);

    // print some info about the display driver
    if (nbf++ % 200 == 0)
    {
        tft.printStats();
        diff1.printStats();
    }
}


/** end of file */
