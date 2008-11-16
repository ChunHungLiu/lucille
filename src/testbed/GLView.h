#include <FL/Fl_Gl_Window.H>
#include <Fl/gl.h>

#include "trackball.h"
#include "glm.h"
#include "BVHVisualizer.h"

class GLView : public Fl_Gl_Window {

    void draw();
    void resize(int x, int y, int w, int h);
    int  handle(int e);
    void restore();

    void glInit();
    void getView( float eye[4], float lookat[4], float up[4] );

    bool handleKey(int k);

  public:

    GLView(int x, int y, int w, int h) : Fl_Gl_Window(x, y, w, h) {

        viewOrg[0]    =   0.0f;
        viewOrg[1]    =   0.0f;
        viewOrg[2]    =   5.0f;

        viewTarget[0] =   0.0f;
        viewTarget[1] =   0.0f;
        viewTarget[2] =   0.0f;

        saveCurrentViewAsDefaultView();

        sceneScale    =   1.0f;

        trackball( currQuat, 0.0, 0.0, 0.0, 0.0 );

        rotX = 0.0f; rotY = 0.0f;
        rotLightX = 0.0f; rotLightY = 0.0f;

        bvhVisualizer = new BVHVisualizer();

        wireMode      = false;

        //
        // Settings for render image
        //
        image            = new unsigned char[3 * w * h];
        floatImage       = new float[3 * w * h];
        floatAccumImage  = new float[3 * w * h];    // for progressive render
        imageWidth  = w;
        imageHeight = h;


        visualizeMode = 0;
        debugPixelSelectionMode = 0;

        progressiveMode = 0;
        invalidateFrame = 0;
        nRendererdFrames = 0;

        onoffDistantLight = 0;
        lightEditingMode = 0;
    }

    void           saveCurrentViewAsDefaultView();
    void           resetView();

    void           renderImage();


    float          viewOrg[3];
    float          viewTarget[3];
    float          sceneScale;

    float          defViewOrg[3];
    float          defViewTarget[3];

    // mouse
    float          currQuat[4];
    float          prevQuat[4];
    int            mouseX, mouseY;
    int            pressed;
    int            pressedButton;

    float          rotX, rotY;
    float          rotLightX, rotLightY;
  
    unsigned char *image;
    float         *floatImage;
    float         *floatAccumImage;
    int            imageWidth;
    int            imageHeight;
    
    // display prop.
    bool           wireMode;

    bool           displayImage;

    GLMmodel      *obj;
    void           setObj(GLMmodel *obj) { this->obj = obj; }

    BVHVisualizer *bvhVisualizer;

    int            visualizeMode;
    int            debugPixelSelectionMode;


    //
    // Progressive rendering?
    //
    int            progressiveMode;
    int            invalidateFrame;
    int            nRendererdFrames;

    int            onoffDistantLight;
    int            lightEditingMode;

};
