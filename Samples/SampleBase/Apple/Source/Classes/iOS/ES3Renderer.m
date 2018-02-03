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
    std::unique_ptr<Renderer> _renderer;
}
@end

@implementation ES3Renderer

// Create an ES 2.0 context
- (instancetype)initWithContext:(EAGLContext*)context AndDrawable:(id<EAGLDrawable>)drawable
{
    _renderer.reset(new Renderer());
    // Init our renderer.
    _renderer->Init((__bridge void*)drawable);
	
	return self;
}

- (void)render
{
    // Replace the implementation of this method to do your own custom drawing

    //glBindFramebuffer(GL_FRAMEBUFFER, 1);
    //glClearColor(1, 1, 0, 0);
    //glClear(GL_COLOR_BUFFER_BIT);
    //glViewport(0,0,400, 600);
    _renderer->Render();
}

- (BOOL)resizeFromLayer:(CAEAGLLayer*)layer
{
    _renderer->WindowResize(0, 0);
	
    return YES;
}

- (void)dealloc
{
    _renderer.reset();
}

@end
