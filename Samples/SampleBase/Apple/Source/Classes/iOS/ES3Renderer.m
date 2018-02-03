/*
 Copyright (C) 2015 Apple Inc. All Rights Reserved.
 See LICENSE.txt for this sample’s licensing information
 
 Abstract:
 The main rendering code.
 */

#import "ES3Renderer.h"

#import <OpenGLES/ES3/gl.h>
#import <OpenGLES/ES3/glext.h>

#include "Renderer.h"
#include <memory>

@interface ES3Renderer ()
{
    EAGLContext* _context;

    // The OpenGL names for the framebuffer and renderbuffer used to render to this view
    GLuint _colorRenderbuffer;
    GLuint _depthRenderbuffer;
    GLuint _defaultFBOName;
    std::unique_ptr<Renderer> _renderer;
}
@end

@implementation ES3Renderer

// Create an ES 2.0 context
- (instancetype)initWithContext:(EAGLContext*)context AndDrawable:(id<EAGLDrawable>)drawable
{
    // Create default framebuffer object. The backing will be allocated for the
    // current layer in -resizeFromLayer
    
	glGenFramebuffers(1, &_defaultFBOName);

	glGenRenderbuffers(1, &_colorRenderbuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, _defaultFBOName);
	glBindRenderbuffer(GL_RENDERBUFFER, _colorRenderbuffer);
	_context = context;
	
	// This call associates the storage for the current render buffer with the
    // EAGLDrawable (our CAEAGLLayer) allowing us to draw into a buffer that
    // will later be rendered to the screen wherever the layer is (which
    // corresponds with our view).
	[_context renderbufferStorage:GL_RENDERBUFFER fromDrawable:drawable];
	
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, _colorRenderbuffer);

    // Get the drawable buffer's width and height so we can create a depth buffer for the FBO
	GLint backingWidth;
	GLint backingHeight;
	glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, &backingWidth);
	glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_HEIGHT, &backingHeight);

    // Create a depth buffer to use with our drawable FBO
	glGenRenderbuffers(1, &_depthRenderbuffer);
	glBindRenderbuffer(GL_RENDERBUFFER, _depthRenderbuffer);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, backingWidth, backingHeight);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, _depthRenderbuffer);
	
	if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	{
		NSLog(@"failed to make complete framebuffer object %x", glCheckFramebufferStatus(GL_FRAMEBUFFER));
		return nil;
	}
    
    void *cdrawable = (__bridge void*)drawable;
    
    _renderer.reset(new Renderer());
    // Init our renderer.
    _renderer->Init();
	
	if (nil == self)
	{
		glDeleteFramebuffers(1, &_defaultFBOName);
		glDeleteRenderbuffers(1, &_colorRenderbuffer);
		glDeleteRenderbuffers(1, &_depthRenderbuffer);
	}
	
	return self;
}

- (void)render
{
    // Replace the implementation of this method to do your own custom drawing

    glBindFramebuffer(GL_FRAMEBUFFER, _defaultFBOName);
    glClearColor(1, 1, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);
    glViewport(0,0,400, 600);
    _renderer->Render();
	    
    glBindRenderbuffer(GL_RENDERBUFFER, _colorRenderbuffer);
    [_context presentRenderbuffer:GL_RENDERBUFFER];
}

- (BOOL)resizeFromLayer:(CAEAGLLayer*)layer
{
	// The pixel dimensions of the CAEAGLLayer
	GLint backingWidth;
	GLint backingHeight;
	
    glBindFramebuffer(GL_FRAMEBUFFER, _defaultFBOName);
	// Allocate color buffer backing based on the current layer size
    glBindRenderbuffer(GL_RENDERBUFFER, _colorRenderbuffer);
    [_context renderbufferStorage:GL_RENDERBUFFER fromDrawable:layer];
	glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, &backingWidth);
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_HEIGHT, &backingHeight);
	
	glGenRenderbuffers(1, &_depthRenderbuffer);
	glBindRenderbuffer(GL_RENDERBUFFER, _depthRenderbuffer);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, backingWidth, backingHeight);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, _depthRenderbuffer);
	
	//[super resizeWithWidth:backingWidth AndHeight:backingHeight];
	
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	{
        NSLog(@"Failed to make complete framebuffer object %x", glCheckFramebufferStatus(GL_FRAMEBUFFER));
        return NO;
    }
	
    return YES;
}

- (void)dealloc
{
	// tear down GL
	if (_defaultFBOName)
	{
		glDeleteFramebuffers(1, &_defaultFBOName);
		_defaultFBOName = 0;
	}
	
	if (_colorRenderbuffer)
	{
		glDeleteRenderbuffers(1, &_colorRenderbuffer);
		_colorRenderbuffer = 0;
	}
    
    _renderer.reset();
}

@end
