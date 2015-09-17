#pragma once
#include "OIT New Coverage Algorithm.h"
#include "OGLErrorHandling.h"


void OITNewCoverage::VInit(GraphicsWindow* window)
{
	m_width = window->getWindowWidth();
	m_height = window->getWindowHeight();

	// Load accumulation shader for accumulating all the transparent matrials, as well as their alphas.
	m_accumTransparencyRevealageShader = new GLSLProgram();
	m_accumTransparencyRevealageShader->initShaderProgram("AccumTransparencyRevealageVert.glsl","","","","AccumTransparencyRevealageFrag.glsl");

	// Load Weighted Average shader, which will be used for final compositing (a variation of screen filling quad shader using multiple textures).
	m_newOITCoverageShader = new GLSLProgram();
	m_newOITCoverageShader->initShaderProgram("NewOITCoverageVert.glsl","","","","NewOITCoverageFrag.glsl");

	// Load screen filling quad shader.
	m_screenFillingQuadShader = new GLSLProgram();
	m_screenFillingQuadShader->initShaderProgram("ScreenFillingQuadVert.glsl","","","","ScreenFillingQuadFrag.glsl");

	// Create framebuffer for tranparent objects overdraw count.
	m_accumFrameBuffer = new Framebuffer(window->getWindowWidth(),window->getWindowHeight());

	m_activeColorAttachments.push_back(0);
	m_activeColorAttachments.push_back(1);

	m_accumFrameBuffer->setColorAttachment(0);
	m_accumFrameBuffer->setColorAttachment(1);

	m_accumFrameBuffer->setDepthStencilTexture();

	m_accumFrameBuffer->unbind();

	// Additional textures to pass for the second transparency render pass.
	m_additionalTexturesTransparency.push_back(m_accumFrameBuffer->getColorAttachment(0));
	m_additionalTexturesTransparency.push_back(m_accumFrameBuffer->getColorAttachment(1));

	// Create screen filling quad.
	m_compositeScreenFillingQuad = new Quad(glm::vec3(-1.0,1.0,0), glm::vec3(-1.0,-1.0,0), glm::vec3(1.0,-1.0,0), glm::vec3(1.0,1.0,0), glm::vec3(0), 0, 0, 0);
	
	m_compositeScreenFillingQuad->initQuad();
	m_compositeScreenFillingQuad->setGLSLProgram(*m_screenFillingQuadShader);
	//m_compositeScreenFillingQuad->setTexture(m_accumFrameBuffer->getColorAttachment(0));

	glGenTextures(1,&m_opaqueTextureHandle);

	glBindTexture(GL_TEXTURE_2D, m_opaqueTextureHandle);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, m_width, m_height, 0, GL_RGBA, GL_FLOAT, NULL);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glBindTexture(GL_TEXTURE_2D, 0);

	m_additionalTexturesOpaque.push_back(m_opaqueTextureHandle);

	glGenTextures(1,&m_resultTexture);

	glBindTexture(GL_TEXTURE_2D, m_resultTexture);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, m_width, m_height, 0, GL_RGBA, GL_FLOAT, NULL);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glBindTexture(GL_TEXTURE_2D, 0);

	check_gl_error();
}

void OITNewCoverage::VAlgorithm(FreeCamera* camera, std::vector<Model*> models, std::vector<Quad*> transparentQuads, 
								Framebuffer* frameBufferOpaque, GLSLProgram* defaultShader, 
								GLuint sampler)
{
	frameBufferOpaque->bind();
	glDisable(GL_CULL_FACE);

	glEnable(GL_ALPHA_TEST);
	glAlphaFunc(GL_GREATER,0.2f);
	
	for (unsigned int i = 0; i < models.size(); i++)
	{
		models[i]->renderTransparentWithAdditionalTextures(m_additionalTexturesOpaque,sampler);
	}
		
	glDisable(GL_ALPHA_TEST);
	frameBufferOpaque->unbind();

	glClearColor(0.0f,0.0f,0.0f,0.0f);
	const float clearColorWhite = 1.0f;

	//_______________________________________________________________________________________________________________________________________________________________________________
	// Acuumulation pass
	m_accumTransparencyRevealageShader->use();
	m_accumTransparencyRevealageShader->setUniform("VPMatrix", camera->getVPMatrix());

	//for (int i = 0; i < models.size(); i++)
	//{
	//	models[i]->setGLSLProgram(m_accumTransparencyRevealageShader);
	//}

	for (int i = 0; i < transparentQuads.size(); i++)
	{
		transparentQuads[i]->setGLSLProgram(*m_accumTransparencyRevealageShader);
	}

	m_accumFrameBuffer->clean();
	m_accumFrameBuffer->bind();
	m_accumFrameBuffer->bindForRenderPass(m_additionalTexturesTransparency);
	m_accumFrameBuffer->cleanColorAttachment(1,clearColorWhite);

	frameBufferOpaque->unbind();

	// Blitting the opaque scene depth to propperly depth test the transparen against it.
	//frameBufferOpaque->bindForReading();

	//frameBufferOpaque->bindForWriting();
	//glBlitFramebuffer(0, 0, m_width, m_height, 0, 0, m_width, m_height, 
	//					GL_DEPTH_BUFFER_BIT, GL_NEAREST);

	//frameBufferOpaque->unbind();

	// Blitting the opaque scene depth to propperly depth test the transparen against it.
	frameBufferOpaque->bindForReading(); 

	glBindTexture(GL_TEXTURE_2D, m_opaqueTextureHandle);
	glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, m_width, m_height);
	glBindTexture(GL_TEXTURE_2D, 0); 

	m_accumFrameBuffer->bindForWriting();
	
	glBlitFramebuffer(0, 0, m_width, m_height, 0, 0, m_width, m_height, 
						GL_DEPTH_BUFFER_BIT, GL_NEAREST); 

	frameBufferOpaque->unbind(); 

	m_additionalTexturesOpaque[0] = m_opaqueTextureHandle;

	m_accumFrameBuffer->bind();
	m_accumFrameBuffer->bindForRenderPass(m_additionalTexturesTransparency);

	glEnable(GL_BLEND);
	glBlendFunci(0, GL_ONE, GL_ONE);
	glBlendFunci(1, GL_ZERO, GL_ONE_MINUS_SRC_ALPHA);
	//glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_FALSE);
	glDisable(GL_CULL_FACE);

	//for (int i = 0; i < models.size(); i++)
	//{
	//	models[i]->renderTransparent();
	//}

	for (int i = 0; i < transparentQuads.size(); i++)
	{
		transparentQuads[i]->render();
	}

	m_accumFrameBuffer->unbind();

	//for (int i = 0; i < models.size(); i++)
	//{
	//	models[i]->setGLSLProgram(defaultShader);
	//}

	for (int i = 0; i < transparentQuads.size(); i++)
	{
		transparentQuads[i]->setGLSLProgram(*defaultShader);
	}

	glDisable(GL_BLEND);
	//glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);

	//_______________________________________________________________________________________________________________________________________________________________________________
	// Final compositing pass

	m_compositeScreenFillingQuad->setGLSLProgram(*m_screenFillingQuadShader);
	m_compositeScreenFillingQuad->getCurrentShaderProgram()->use();
	m_compositeScreenFillingQuad->renderWithAdditionalTextures(m_additionalTexturesOpaque,sampler);

	m_newOITCoverageShader->use();

	glDepthMask(GL_FALSE);
	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE_MINUS_SRC_ALPHA, GL_SRC_ALPHA);

	m_compositeScreenFillingQuad->renderWithAdditionalTextures(m_additionalTexturesTransparency,sampler);

	glDepthMask(GL_TRUE);
	glDisable(GL_BLEND);
	//_______________________________________________________________________________________________________________________________________________________________________________

	glBindTexture(GL_TEXTURE_2D, m_resultTexture);
	glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, m_width, m_height);
	glBindTexture(GL_TEXTURE_2D, 0);

	check_gl_error();
}