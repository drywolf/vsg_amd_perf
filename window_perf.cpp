// based on the code of:
// https://github.com/vsg-dev/vsgExamples/blob/9d3ae99f4362d271d3a16e3f4ad50a627ada6667/examples/app/vsghelloworld/vsghelloworld.cpp

#include <vsg/all.h>

#include <chrono>
#include <iostream>

using namespace std::chrono_literals;

int main(int argc, char** argv)
{
    // set up defaults and read command line arguments to override them
    vsg::CommandLine arguments(&argc, argv);

    if (arguments.errors()) return arguments.writeErrorMessages(std::cerr);

    auto vsg_scene = vsg::MatrixTransform::create();

    // create the viewer and assign window(s) to it
    auto windowTraits = vsg::WindowTraits::create();
    windowTraits->windowTitle = "Hello World";
    windowTraits->apiDumpLayer = arguments.read({"--api", "-a"});
    windowTraits->debugLayer = arguments.read({"--debug", "-d"});
    windowTraits->fullscreen = arguments.read("--fullscreen");

    // show current FPS in window title-bar
    windowTraits->requestedLayers.push_back("VK_LAYER_LUNARG_monitor");

    // IMPORTANT: unlocked framerate / no VSync
    windowTraits->swapchainPreferences.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;

    auto viewer = vsg::Viewer::create();
    auto window = vsg::Window::create(windowTraits);
    if (!window)
    {
        std::cout << "Could not create window." << std::endl;
        return 1;
    }

    viewer->addWindow(window);

    // compute the bounds of the scene graph to help position camera
    vsg::ComputeBounds computeBounds;
    vsg_scene->accept(computeBounds);
    vsg::dvec3 centre = (computeBounds.bounds.min + computeBounds.bounds.max) * 0.5;
    double radius = vsg::length(computeBounds.bounds.max - computeBounds.bounds.min) * 0.6;
    double nearFarRatio = 0.001;

    // set up the camera
    auto lookAt = vsg::LookAt::create(centre + vsg::dvec3(0.0, -radius * 3.5, 0.0), centre, vsg::dvec3(0.0, 0.0, 1.0));

    vsg::ref_ptr<vsg::ProjectionMatrix> perspective;
    auto ellipsoidModel = vsg_scene->getRefObject<vsg::EllipsoidModel>("EllipsoidModel");
    if (ellipsoidModel)
    {
        double horizonMountainHeight = 0.0;
        perspective = vsg::EllipsoidPerspective::create(lookAt, ellipsoidModel, 30.0, static_cast<double>(window->extent2D().width) / static_cast<double>(window->extent2D().height), nearFarRatio, horizonMountainHeight);
    }
    else
    {
        perspective = vsg::Perspective::create(30.0, static_cast<double>(window->extent2D().width) / static_cast<double>(window->extent2D().height), nearFarRatio * radius, radius * 4.5);
    }

    auto camera = vsg::Camera::create(perspective, lookAt, vsg::ViewportState::create(window->extent2D()));

    // add close handler to respond to the close window button and pressing escape
    viewer->addEventHandler(vsg::CloseHandler::create(viewer));

    // add trackball to control the Camera
    viewer->addEventHandler(vsg::Trackball::create(camera, ellipsoidModel));

    // add the CommandGraph to render the scene
    auto commandGraph = vsg::createCommandGraphForView(window, camera, vsg_scene);
    viewer->assignRecordAndSubmitTaskAndPresentation({commandGraph});

    // compile all Vulkan objects and transfer image, vertex and primitive data to GPU
    viewer->compile();

    auto start = std::chrono::high_resolution_clock::now();
    uint32_t rendered_frames = 0;

    // rendering main loop
    while (viewer->advanceToNextFrame())
    {
        // pass any events into EventHandlers assigned to the Viewer
        viewer->handleEvents();

        viewer->update();

        viewer->recordAndSubmit();

        viewer->present();

        ++rendered_frames;

        auto now = std::chrono::high_resolution_clock::now();
        std::chrono::microseconds duration_us = std::chrono::duration_cast<std::chrono::microseconds>(now - start);
        if (duration_us >= 1s)
        {
            double fps = rendered_frames / double(duration_us.count()) * 1000000.0;
            std::cout << "FPS: " << fps << std::endl;

            rendered_frames = 0;
            start = std::chrono::high_resolution_clock::now();
        }
    }

    // clean up done automatically thanks to ref_ptr<>
    return 0;
}
