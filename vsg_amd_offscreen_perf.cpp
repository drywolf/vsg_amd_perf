// based on the code of:
// https://github.com/vsg-dev/vsgExamples/blob/9d3ae99f4362d271d3a16e3f4ad50a627ada6667/examples/app/vsgheadless/vsgheadless.cpp

#include <vsg/all.h>

#include <chrono>
#include <iostream>
#include <thread>

using namespace std::chrono_literals;

vsg::ref_ptr<vsg::ImageView> createColorImageView(vsg::ref_ptr<vsg::Device> device, const VkExtent2D& extent, VkFormat imageFormat, VkSampleCountFlagBits samples)
{
    auto colorImage = vsg::Image::create();
    colorImage->imageType = VK_IMAGE_TYPE_2D;
    colorImage->format = imageFormat;
    colorImage->extent = VkExtent3D{extent.width, extent.height, 1};
    colorImage->mipLevels = 1;
    colorImage->arrayLayers = 1;
    colorImage->samples = samples;
    colorImage->tiling = VK_IMAGE_TILING_OPTIMAL;
    colorImage->usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    colorImage->initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorImage->flags = 0;
    colorImage->sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    return vsg::createImageView(device, colorImage, VK_IMAGE_ASPECT_COLOR_BIT);
}

vsg::ref_ptr<vsg::RenderPass> createColorRenderPass(vsg::Device* device, VkFormat imageFormat)
{
    vsg::AttachmentDescription colorAttachment = {};
    colorAttachment.format = imageFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    vsg::RenderPass::Attachments attachments{colorAttachment};

    vsg::AttachmentReference colorAttachmentRef = {};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    vsg::SubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachments.emplace_back(colorAttachmentRef);

    vsg::RenderPass::Subpasses subpasses{subpass};

    // image layout transition
    vsg::SubpassDependency colorDependency = {};
    colorDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    colorDependency.dstSubpass = 0;
    colorDependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    colorDependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    colorDependency.srcAccessMask = 0;
    colorDependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    colorDependency.dependencyFlags = 0;

    vsg::RenderPass::Dependencies dependencies{colorDependency};

    return vsg::RenderPass::create(device, attachments, subpasses, dependencies);
}

int main(int argc, char** argv)
{
    // set up defaults and read command line arguments to override them
    VkExtent2D extent{2048, 1024};
    VkFormat imageFormat = VK_FORMAT_R8G8B8A8_UNORM;

    vsg::CommandLine arguments(&argc, argv);
    arguments.read({"--extent", "-w"}, extent.width, extent.height);
    auto debugLayer = arguments.read({"--debug", "-d"});
    auto apiDumpLayer = arguments.read({"--api", "-a"});
    auto numFrames = arguments.value(std::numeric_limits<uint32_t>::max(), "-n");
    if (arguments.read("--st")) extent = VkExtent2D{192, 108};
    bool above = arguments.read("--above");
    bool enableGeometryShader = arguments.read("--gs");
    bool logFrame = arguments.read("-f");

    if (!arguments.read("--details"))
        putenv("VK_APIDUMP_DETAILED=false");

    if (arguments.read("--logfile"))
        putenv("VK_APIDUMP_LOG_FILENAME=vk_api.log");

    if (arguments.errors()) return arguments.writeErrorMessages(std::cerr);

    uint32_t vulkanVersion = VK_API_VERSION_1_3;

    auto vsg_scene = vsg::MatrixTransform::create(); // empty scene!

    // create instance
    vsg::Names instanceExtensions;
    vsg::Names requestedLayers;
    if (debugLayer || apiDumpLayer)
    {
        instanceExtensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
        instanceExtensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
        requestedLayers.push_back("VK_LAYER_KHRONOS_validation");
        if (apiDumpLayer) requestedLayers.push_back("VK_LAYER_LUNARG_api_dump");
    }

    vsg::Names validatedNames = vsg::validateInstancelayerNames(requestedLayers);

    auto instance = vsg::Instance::create(instanceExtensions, validatedNames, vulkanVersion);
    auto [physicalDevice, queueFamily] = instance->getPhysicalDeviceAndQueueFamily(VK_QUEUE_GRAPHICS_BIT);
    if (!physicalDevice || queueFamily < 0)
    {
        std::cout << "Could not create PhysicalDevice" << std::endl;
        return 0;
    }

    vsg::Names deviceExtensions;
    vsg::QueueSettings queueSettings{vsg::QueueSetting{queueFamily, {1.0}}};

    auto deviceFeatures = vsg::DeviceFeatures::create();
    deviceFeatures->get().samplerAnisotropy = VK_TRUE;
    deviceFeatures->get().geometryShader = enableGeometryShader;

    auto device = vsg::Device::create(physicalDevice, queueSettings, validatedNames, deviceExtensions, deviceFeatures);

    // compute the bounds of the scene graph to help position camera
    vsg::ComputeBounds computeBounds;
    vsg_scene->accept(computeBounds);
    vsg::dvec3 centre = (computeBounds.bounds.min + computeBounds.bounds.max) * 0.5;
    double radius = vsg::length(computeBounds.bounds.max - computeBounds.bounds.min) * 0.6;
    double nearFarRatio = 0.001;

    // set up the camera
    auto lookAt = (above) ? vsg::LookAt::create(centre + vsg::dvec3(0.0, 0.0, radius * 1.5), centre, vsg::dvec3(0.0, 1.0, 0.0)) : vsg::LookAt::create(centre + vsg::dvec3(0.0, -radius * 1.5, 0.0), centre, vsg::dvec3(0.0, 0.0, 1.0));

    vsg::ref_ptr<vsg::ProjectionMatrix> perspective;
    if (auto ellipsoidModel = vsg_scene->getRefObject<vsg::EllipsoidModel>("EllipsoidModel"))
    {
        perspective = vsg::EllipsoidPerspective::create(lookAt, ellipsoidModel, 30.0, static_cast<double>(extent.width) / static_cast<double>(extent.height), nearFarRatio, 0.0);
    }
    else
    {
        perspective = vsg::Perspective::create(30.0, static_cast<double>(extent.width) / static_cast<double>(extent.height), nearFarRatio * radius, radius * 4.5);
    }

    auto camera = vsg::Camera::create(perspective, lookAt, vsg::ViewportState::create(extent));

    vsg::ref_ptr<vsg::ImageView> colorImageView = createColorImageView(device, extent, imageFormat, VK_SAMPLE_COUNT_1_BIT);

    auto renderPass = createColorRenderPass(device, imageFormat);
    vsg::ref_ptr<vsg::Framebuffer> framebuffer = vsg::Framebuffer::create(renderPass, vsg::ImageViews{ colorImageView }, extent.width, extent.height, 1);

    auto renderGraph = vsg::RenderGraph::create();

    renderGraph->framebuffer = framebuffer;
    renderGraph->renderArea.offset = {0, 0};
    renderGraph->renderArea.extent = extent;
    renderGraph->setClearValues({{1.0f, 1.0f, 0.0f, 0.0f}}, VkClearDepthStencilValue{0.0f, 0});

    auto view = vsg::View::create(camera, vsg_scene);

    vsg::CommandGraphs commandGraphs;
    renderGraph->addChild(view);

    auto commandGraph = vsg::CommandGraph::create(device, queueFamily);
    commandGraph->addChild(renderGraph);
    commandGraphs.push_back(commandGraph);

    // create the viewer
    auto viewer = vsg::Viewer::create();

    viewer->assignRecordAndSubmitTaskAndPresentation(commandGraphs);

    viewer->compile();

    auto start = std::chrono::high_resolution_clock::now();
    uint32_t rendered_frames = 0;

    // rendering main loop
    while ((numFrames--) > 0)
    {
        if (logFrame)
            std::cout << "> BEGIN FRAME -----------------------------------------------------------------------------------------------------------------------------------" << std::endl;

        if (!viewer->advanceToNextFrame())
            break;

        // pass any events into EventHandlers assigned to the Viewer, this includes Frame events generated by the viewer each frame
        viewer->handleEvents();

        viewer->update();

        viewer->recordAndSubmit();

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

        if (logFrame)
            std::cout << "> END FRAME -----------------------------------------------------------------------------------------------------------------------------------" << std::endl;
    }

    // clean up done automatically thanks to ref_ptr<>
    return 0;
}
