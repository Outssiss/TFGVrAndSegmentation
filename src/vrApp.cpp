#define STB_IMAGE_IMPLEMENTATION
#include "vrApp.h"

using namespace glm;

void GLAPIENTRY MessageCallback(GLenum source,
    GLenum type,
    GLuint id,
    GLenum severity,
    GLsizei length,
    const GLchar* message,
    const void* userParam)
{
    fprintf(stderr, "GL CALLBACK: %s type = 0x%x, severity = 0x%x, message = %s\n",
        (type == GL_DEBUG_TYPE_ERROR ? "** GL ERROR **" : ""),
        type, severity, message);
}

bool openvr::init() { 

	glfwInit();
    width = 640; height = 320;

	window = glfwCreateWindow(width, height, "Opengl", nullptr, nullptr);
	glfwMakeContextCurrent(window);
    
	glewExperimental = GL_TRUE;
	GLenum nGlewError = glewInit();

    glEnable(GL_DEBUG_OUTPUT);
    glDebugMessageCallback(MessageCallback, 0);

	if (nGlewError != GLEW_OK)
	{
		printf("%s - Error initializing GLEW! %s\n", __FUNCTION__, glewGetErrorString(nGlewError));
		return false;
	}
	glGetError(); // to clear the error caused deep in GLEW

	vr::EVRInitError eError = vr::VRInitError_None;
	m_pHMD = vr::VR_Init(&eError, vr::VRApplication_Scene);
	if (eError != vr::VRInitError_None)
	{
		m_pHMD = NULL;
		char buf[1024];
		sprintf_s(buf, sizeof(buf), "Unable to init VR runtime: %s", vr::VR_GetVRInitErrorAsEnglishDescription(eError));
		return false;
	}

    m_rTrackingUniverse = vr::ETrackingUniverseOrigin::TrackingUniverseStanding;
    vr::VRCompositor()->SetTrackingSpace(m_rTrackingUniverse);

	m_fNearClip = 0.01f;
	m_fFarClip = 30.0f;

    m_model = Matrix4();
	//initGL
    if (!setupFrontCameras())
    {
        printf("Error during front cameras setup");
        return false;
    }
    else
    {
        printf("Front cameras initialized correctly");
    }

	//init Compositor
	vr::EVRInitError peError = vr::VRInitError_None;

	if (!vr::VRCompositor())
	{
		printf("Compositor initialization failed. See log file for details\n");
		return false;
	}

    createShader();
    setupScene();
    setupMatrix();
    setupStereoRenderTargets();

	return true;
}

void openvr::createShader() {
    Shader lshader_quad("res/shaders/fbovertex.vert", "res/shaders/fbofragment.frag");
    Shader lshader_square("res/shaders/quad.vert", "res/shaders/quad.frag");
	
    shader_quad = lshader_quad.ID;
    shader_square = lshader_square.ID; //TFG

    m_nQuadCameraMatrixLocation = glGetUniformLocation(shader_square, "matrix");
    m_nFrontCamerasMatrixLocation = glGetUniformLocation(shader_square, "frontCameraMatrix");
}

void openvr::setupQuadCamera()
{

    while (glGetError() != GL_NO_ERROR) {

    }

    float vertices[] = {
        2.5f,  2.5f, -1.0f,  1.0f, 1.0f, 1.0f, 1.0f, 1.0f, // top right
        2.5f, -2.5f, -1.0f,  1.0f, 0.0f, 1.0f, 1.0f, 1.0f, // bottom right
       -2.5f, -2.5f, -1.0f,  0.0f, 0.0f, 1.0f, 1.0f, 1.0f, // bottom left
       -2.5f,  2.5f, -1.0f,  0.0f, 1.0f, 1.0f, 1.0f, 1.0f  // top left
    };


    unsigned int indices[] = {  // note that we start from 0!
        0, 1, 3,  // first Triangle
        1, 2, 3   // second Triangle
    };

    unsigned int VBO, VAO, EBO;
    glGenVertexArrays(1, &vao_square);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(vao_square);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    GLint posCamAttrib = glGetAttribLocation(shader_square, "position");
    glVertexAttribPointer(posCamAttrib, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(posCamAttrib);

    // color coords
    GLint colCamAttrib = glGetAttribLocation(shader_square, "color");
    glVertexAttribPointer(colCamAttrib, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(5 * sizeof(float)));
    glEnableVertexAttribArray(colCamAttrib);

    //texcoords
    GLint texAttrib = glGetAttribLocation(shader_square, "texcoord");
    glVertexAttribPointer(texAttrib, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(texAttrib);

    //sampler uniform
    GLint texpos = glGetAttribLocation(shader_square, "tex");
    glUniform1i(texpos, 0);


    //glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

}

GLuint openvr::loadTextureFromMat(cv::Mat& img) {
    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);

    // Flip the image vertically before uploading to OpenGL
    cv::flip(img, img, 0);

    // Upload the image data to OpenGL
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, img.cols, img.rows, 0, GL_RGBA, GL_UNSIGNED_BYTE, img.data);
    glGenerateMipmap(GL_TEXTURE_2D);

    return texture;
}

void openvr::updateTextureFromMat(cv::Mat& img, GLuint texture) {
    glBindTexture(GL_TEXTURE_2D, texture);

    // Flip the image vertically before uploading to OpenGL
    cv::flip(img, img, 0);

    // Update the image data in OpenGL
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, img.cols, img.rows, GL_RGBA, GL_UNSIGNED_BYTE, img.data);
    glGenerateMipmap(GL_TEXTURE_2D);
}

GLuint openvr::loadTextureFromImage(const char* texturePath) {
    	GLuint texture;
	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);

	int width, height, nrChannels;
    stbi_set_flip_vertically_on_load(true);
    unsigned char* data = stbi_load(texturePath, &width, &height, &nrChannels, 0);

    if (data)
    {
		GLenum format;
		if (nrChannels == 1)
			format = GL_RED;
		else if (nrChannels == 3)
			format = GL_RGB;
		else if (nrChannels == 4)
			format = GL_RGBA;

		glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
		glGenerateMipmap(GL_TEXTURE_2D);

		stbi_image_free(data);
	}
    else
    {
		std::cout << "Failed to load texture" << std::endl;
	}

	return texture;
}

void openvr::setupScene() {
  //companion window

  glUseProgram(shader_quad);
  GLfloat screenQuadVertices[] =
  {//	x		y	r	g	b,  u, v
    -1.0, -1.0, 1.0, 1.0, 1.0, 0.0, 0.0,
    1.0, -1.0, 1.0, 1.0, 1.0, 1.0, 0.0,
    1.0,  1.0, 1.0, 1.0, 1.0, 1.0, 1.0,
    -1.0,  1.0, 1.0, 1.0, 1.0, 0.0, 1.0
  };

  //vertex Attributes
  GLuint vbo2;
  glGenBuffers(1, &vbo2);
  glBindBuffer(GL_ARRAY_BUFFER, vbo2);
  glBufferData(GL_ARRAY_BUFFER, sizeof(screenQuadVertices), screenQuadVertices, GL_DYNAMIC_DRAW);

  //load layout location of position
  GLint posAttrib2 = glGetAttribLocation(shader_quad, "position");
  glVertexAttribPointer(posAttrib2, 2, GL_FLOAT, GL_FALSE, 7 * sizeof(GL_FLOAT), 0);
  glEnableVertexAttribArray(posAttrib2);

  //color
  GLint colAttrib2 = glGetAttribLocation(shader_quad, "color");
  glVertexAttribPointer(colAttrib2, 3, GL_FLOAT, GL_FALSE, 7 * sizeof(GL_FLOAT), (void*)(sizeof(GL_FLOAT) * 2));
  glEnableVertexAttribArray(colAttrib2);

  //texcoords
  GLint texAttrib2 = glGetAttribLocation(shader_quad, "texcoord");
  glVertexAttribPointer(texAttrib2, 2, GL_FLOAT, GL_FALSE, 7 * sizeof(GL_FLOAT), (void*)(sizeof(GL_FLOAT) * 5));
  glEnableVertexAttribArray(texAttrib2);

  //sampler uniform
  GLint texpos2 = glGetAttribLocation(shader_quad, "tex");
  glUniform1i(texpos2, 0);

  glBindVertexArray(0);

  setupQuadCamera();
}

void openvr::setupMatrix() {

	m_mat4ProjectionLeft = GetHMDMatrixProjectionEye(vr::Eye_Left);
	m_mat4ProjectionRight = GetHMDMatrixProjectionEye(vr::Eye_Right);

	m_mat4eyePosLeft = GetHMDMatrixPoseEye(vr::Eye_Left);
	m_mat4eyePosRight = GetHMDMatrixPoseEye(vr::Eye_Right);

    m_mat4HMDPose = Matrix4(); //Init with identity

}

bool openvr::setupFrontCameras()
{
    m_CameraFrameType = vr::VRTrackedCameraFrameType_Undistorted;
    
    bool bHasCamera = false;
    vr::EVRTrackedCameraError nCameraError = vr::VRTrackedCamera()->HasCamera(vr::k_unTrackedDeviceIndex_Hmd, &bHasCamera);

    if (nCameraError != vr::VRTrackedCameraError_None)
    {
        printf("Tracked Camera Error! (%s)\n", vr::VRTrackedCamera()->GetCameraErrorNameFromEnum(nCameraError));
        return false;
    }

    if (!bHasCamera)
    {
        printf("No Tracked Camera Available!\n");
        return false;
    }

    vr::ETrackedPropertyError propertyError;
    char buffer[128];
    vr::VRSystem()->GetStringTrackedDeviceProperty(vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_CameraFirmwareDescription_String, buffer, sizeof(buffer), &propertyError);

    uint32_t nNewBufferSize = 0;
    vr::VRTrackedCamera()->GetCameraFrameSize(vr::k_unTrackedDeviceIndex_Hmd, m_CameraFrameType, &m_nCameraFrameWidth, &m_nCameraFrameHeight, &nNewBufferSize) != vr::VRTrackedCameraError_None;

    if (nNewBufferSize && nNewBufferSize != m_nCameraFrameBufferSize)
    {
        delete[] m_pCameraFrameBuffer;
        m_nCameraFrameBufferSize = nNewBufferSize;
        m_pCameraFrameBuffer = new uint8_t[m_nCameraFrameBufferSize];
        memset(m_pCameraFrameBuffer, 0, m_nCameraFrameBufferSize);
    }

    vr::VRTrackedCamera()->AcquireVideoStreamingService(vr::k_unTrackedDeviceIndex_Hmd, &m_hTrackedCamera);
    return true;

}

void openvr::updateCameraFrameBuffer()
{
    vr::EVRTrackedCameraError error;
    vr::CameraVideoStreamFrameHeader_t header;

    mtx.lock();
    error = vr::VRTrackedCamera()->GetVideoStreamFrameBuffer(m_hTrackedCamera, m_CameraFrameType, m_pCameraFrameBuffer, m_nCameraFrameBufferSize, &header, sizeof(header));
    mtx.unlock();

    if (error == vr::EVRTrackedCameraError::VRTrackedCameraError_None)
    {
        memcpy(&m_CurrentFrameHeader, &header, sizeof(header));
    }
}

Matrix4 openvr::GetHMDMatrixProjectionEye(vr::Hmd_Eye nEye) {

	vr::HmdMatrix44_t mat = m_pHMD->GetProjectionMatrix(nEye, m_fNearClip, m_fFarClip);


    Matrix4 mat4OpenVR = Matrix4(mat.m[0][0], mat.m[1][0], mat.m[2][0], mat.m[3][0],
                                 mat.m[0][1], mat.m[1][1], mat.m[2][1], mat.m[3][1],
                                 mat.m[0][2], mat.m[1][2], mat.m[2][2], mat.m[3][2],
                                 mat.m[0][3], mat.m[1][3], mat.m[2][3], mat.m[3][3]);

    return mat4OpenVR;
}

Matrix4 openvr::GetHMDMatrixPoseEye(vr::Hmd_Eye nEye)
{

	vr::HmdMatrix34_t matEye = m_pHMD->GetEyeToHeadTransform(nEye);

    Matrix4 mat4OpenVR = Matrix4(matEye.m[0][0], matEye.m[1][0], matEye.m[2][0], 0.0,
                                 matEye.m[0][1], matEye.m[1][1], matEye.m[2][1], 0.0,
                                 matEye.m[0][2], matEye.m[1][2], matEye.m[2][2], 0.0,
                                 matEye.m[0][3], matEye.m[1][3], matEye.m[2][3], 1.0f);

    return mat4OpenVR.invert();
}

void openvr::setupStereoRenderTargets() {

	m_pHMD->GetRecommendedRenderTargetSize(&m_nRenderWidth, &m_nRenderHeight);

	createFrameBuffer(m_nRenderWidth, m_nRenderHeight, leftEyeDesc);
	createFrameBuffer(m_nRenderWidth, m_nRenderHeight, rightEyeDesc);

}

void openvr::createFrameBuffer(int nWidth, int nHeight, FramebufferDesc &framebufferDesc) {

	glGenFramebuffers(1, &framebufferDesc.m_nRenderFramebufferId);
	glBindFramebuffer(GL_FRAMEBUFFER, framebufferDesc.m_nRenderFramebufferId);

	glGenTextures(1, &framebufferDesc.m_nRenderTextureId);
	glBindTexture(GL_TEXTURE_2D, framebufferDesc.m_nRenderTextureId);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, nWidth, nHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, framebufferDesc.m_nRenderTextureId, 0);

	// check FBO status
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE) {
		std::cout << "Framebuffer creation successful." << std::endl;
	}
	else {
		std::cout << "Framebuffer creation FAILED." << std::endl;
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);


}

Matrix4 openvr::getCurrentViewProjectionMatrix(vr::Hmd_Eye nEye)
{
  
    Matrix4 matMVP = Matrix4();

    if (nEye == vr::Eye_Left)
    {
        matMVP = m_mat4ProjectionLeft * m_mat4eyePosLeft;

    }
    else if (nEye == vr::Eye_Right)
    {
        matMVP = m_mat4ProjectionRight * m_mat4eyePosRight;
    }

    return matMVP;
}

void openvr::renderScene(vr::Hmd_Eye nEye) {

    if (glGetError() != GL_NO_ERROR) { std::cout << "Error in render scene: " << glGetError() << std::endl; }

    while (glGetError() != GL_NO_ERROR){}

    glUseProgram(shader_square);
    Matrix4 currentViewProjectionMatrix = getCurrentViewProjectionMatrix(nEye);
    glUniformMatrix4fv(m_nQuadCameraMatrixLocation, 1, GL_FALSE, currentViewProjectionMatrix.get());

    glBindVertexArray(vao_square);

    if (nEye == vr::Eye_Left) {
        glBindTexture(GL_TEXTURE_2D, textureLeft);
    }
    else if (nEye == vr::Eye_Right) {
        glBindTexture(GL_TEXTURE_2D, textureRight);
    }
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
  
}

void openvr::renderStereoTargets() {

    updateTextureFromMat(imageRight, textureRight);
    updateTextureFromMat(imageLeft, textureLeft); 

    glViewport(0, 0, m_nRenderWidth, m_nRenderHeight);

    glBindFramebuffer(GL_FRAMEBUFFER, rightEyeDesc.m_nRenderFramebufferId);
    glBindTexture(GL_TEXTURE_2D, rightEyeDesc.m_nRenderTextureId);
    renderScene(vr::Eye_Right);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glBindFramebuffer(GL_FRAMEBUFFER, leftEyeDesc.m_nRenderFramebufferId);
    glBindTexture(GL_TEXTURE_2D, leftEyeDesc.m_nRenderTextureId);
    renderScene(vr::Eye_Left);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

}

Matrix4 openvr::ConvertSteamVRMatrixToMatrix4(const vr::HmdMatrix34_t& matPose)
{
    Matrix4 matrixObj(
        matPose.m[0][0], matPose.m[1][0], matPose.m[2][0], 0.0,
        matPose.m[0][1], matPose.m[1][1], matPose.m[2][1], 0.0,
        matPose.m[0][2], matPose.m[1][2], matPose.m[2][2], 0.0,
        matPose.m[0][3], matPose.m[1][3], matPose.m[2][3], 1.0f
    );
    return matrixObj;
}

void openvr::updateHMDMatrixPose() {

  vr::VRCompositor()->WaitGetPoses(m_rTrackedDevicePose, vr::k_unMaxTrackedDeviceCount, NULL, 0);
  
  m_rmat4DevicePose[vr::k_unTrackedDeviceIndex_Hmd] = ConvertSteamVRMatrixToMatrix4(m_rTrackedDevicePose[vr::k_unTrackedDeviceIndex_Hmd].mDeviceToAbsoluteTracking);
     
  if (m_rTrackedDevicePose[vr::k_unTrackedDeviceIndex_Hmd].bPoseIsValid)
  {
    m_mat4HMDPose = m_rmat4DevicePose[vr::k_unTrackedDeviceIndex_Hmd];
    m_mat4HMDPose = m_mat4HMDPose.invert();
  }

}

void openvr::drawScreenQuad() {
    glViewport(0, 0, width/2, height);

    glBindTexture(GL_TEXTURE_2D, rightEyeDesc.m_nRenderTextureId);
    glUseProgram(shader_quad);
    glBindVertexArray(vao_quad);
    glDrawArrays(GL_POLYGON, 0, 4);

    glViewport(width/2, 0, width/2, height);

    glBindTexture(GL_TEXTURE_2D, leftEyeDesc.m_nRenderTextureId);
    glUseProgram(shader_quad);
    glBindVertexArray(vao_quad);
    glDrawArrays(GL_POLYGON, 0, 4);

    glBindVertexArray(0);

}

std::string openvr::GetTrackedDeviceString(vr::TrackedDeviceIndex_t unDevice, vr::TrackedDeviceProperty prop, vr::TrackedPropertyError *peError)
{
  uint32_t unRequiredBufferLen = vr::VRSystem()->GetStringTrackedDeviceProperty(unDevice, prop, NULL, 0, peError);
  if (unRequiredBufferLen == 0)
    return "";

  char *pchBuffer = new char[unRequiredBufferLen];
  unRequiredBufferLen = vr::VRSystem()->GetStringTrackedDeviceProperty(unDevice, prop, pchBuffer, unRequiredBufferLen, peError);
  std::string sResult = pchBuffer;
  delete[] pchBuffer;
  return sResult;
}

void openvr::separateImagesFromCameraBuffer(const uint8_t* buffer, int width, int height)
{
    cv::Mat imageRightAux(height/2, width, CV_8UC4, (void*)buffer);
    cv::Mat imageLeftAux(height/2, width, CV_8UC4, (void*)(buffer + (width * height * 4) / 2));

    imageLeft = imageLeftAux.clone();
    imageRight = imageRightAux.clone();
}

void openvr::runMainLoop(){

    startSendMQTTMessageThread(mosq);

    vr::EVRCompositorError submitError;

	while (true) {

        if (firstImage) {
            separateImagesFromCameraBuffer(m_pCameraFrameBufferMQTT, 1224, 1840);
        }
        else {
            updateCameraFrameBuffer();

            separateImagesFromCameraBuffer(m_pCameraFrameBuffer, 1224, 1840);

            textureLeft = openvr::loadTextureFromMat(imageLeft);
            textureRight = openvr::loadTextureFromMat(imageRight);
        }
	    renderStereoTargets();

        drawScreenQuad();

        //Create left OpenVR texture
		vr::Texture_t leftEyeTexture = { (void*)(uintptr_t)leftEyeDesc.m_nRenderTextureId, vr::TextureType_OpenGL, vr::ColorSpace_Gamma };
        //Submit left textura plus error checking
        submitError = vr::VRCompositor()->Submit(vr::Eye_Left, &leftEyeTexture);
        if (submitError != vr::VRCompositorError_None) {
            printf("Error when submitting: %d ", submitError);
        }

        //Create right OpenVR texture
		vr::Texture_t rightEyeTexture = { (void*)(uintptr_t)rightEyeDesc.m_nRenderTextureId, vr::TextureType_OpenGL, vr::ColorSpace_Gamma };
        //Submit right textura plus error checking
        submitError = vr::VRCompositor()->Submit(vr::Eye_Right, &rightEyeTexture);
        if (submitError != vr::VRCompositorError_None) {
            printf("Error when submitting: %d ", submitError);
        }

		glfwSwapBuffers(window);
		glfwPollEvents();

        updateCameraFrameBuffer();//Upddate images for next iteration
    
	}

    mosquitto_destroy(mosq);
    mosquitto_lib_cleanup();

}

//MQTT

void on_connect(struct mosquitto* mosq, void* obj, int reason_code)
{
    int error = mosquitto_subscribe(mosq, NULL, topic_receive, 1);
    if (error != MOSQ_ERR_SUCCESS) {
		std::cout << "Error: Unable to subscribe." << std::endl;
    }
    else {
        std::cout << "Connected to MQTT broker." << std::endl;
    }
    
}

void on_message(struct mosquitto* mosq, void* obj, const struct mosquitto_message* message)
{
    size_t payload_size = message->payloadlen;
    m_pCameraFrameBufferMQTT = (uint8_t*)malloc(payload_size);
    std::memcpy(m_pCameraFrameBufferMQTT, message->payload, payload_size);
    firstImage = true;
    ready = true;
}
 
int openvr::mqttSetup() {
    
    mosquitto_lib_init();

    mosq = mosquitto_new(NULL, true, NULL);

    if (!mosq) {
        std::cout << "Error: Out of memory." << std::endl;
        return 1;
    }

    mosquitto_connect_callback_set(mosq, on_connect);
    mosquitto_message_callback_set(mosq, on_message);

    if (mosquitto_connect(mosq, "localhost", 1883, 60)) {
        std::cout << "Unable to connect." << std::endl;
        return 1;
    }
}

void openvr::sendMQTTMessage(struct mosquitto* mosq, const char* payload) {

    mosquitto_loop_start(mosq);

    mosquitto_publish(mosq, NULL, topic_send, m_nCameraFrameBufferSize, payload, 0, false);

    if (mosq == NULL) {
		std::cout << "Error: Out of memory." << std::endl;
		return;
	}

}

void openvr::sendMQTTMessageThread(struct mosquitto* mosq) {
    while (true) {
        if (ready) {
            mtx.lock();
            const char* payload = (char*)m_pCameraFrameBuffer;
            mtx.unlock();
        
            sendMQTTMessage(mosq, payload);

            ready = false;
        }
    }
}

void openvr::startSendMQTTMessageThread(struct mosquitto* mosq) {
    std::thread mqttThread(&openvr::sendMQTTMessageThread, this, mosq);
    mqttThread.detach();  // Detach the thread, allowing it to run independently
}

//MQTT

int main() {

	openvr *op = new openvr();

	op->init();

    op->mqttSetup();

	op->runMainLoop();

    return 0;
}
