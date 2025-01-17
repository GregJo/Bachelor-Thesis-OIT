#pragma once
#include "OGLErrorHandling.h"
#include "Logger.h"
#include <iostream>
#include <string>
#include "glew.h"
 
void _check_gl_error(const char *file, int line) {
#ifdef _DEBUG
        GLenum err (glGetError());
		
        while(err!=GL_NO_ERROR) {
                std::string error;
				std::cout << "Compiler was here!" <<std::endl;
                switch(err) {
                        case GL_INVALID_OPERATION:				error="INVALID_OPERATION";      break;
                        case GL_INVALID_ENUM:					error="INVALID_ENUM";           break;
                        case GL_INVALID_VALUE:					error="INVALID_VALUE";          break;
                        case GL_OUT_OF_MEMORY:					error="OUT_OF_MEMORY";          break;
                        case GL_INVALID_FRAMEBUFFER_OPERATION:  error="INVALID_FRAMEBUFFER_OPERATION";  break;
                }
 
                std::cerr << "GL_" << error.c_str() <<" - "<<file<<":"<<line<<std::endl;

				Logger::GetInstance().Log("OpenGL Error at line: %d", line);

                err=glGetError();
        }
#endif
}