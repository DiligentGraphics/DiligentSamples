/*
 Copyright (C) 2015 Apple Inc. All Rights Reserved.
 See LICENSE.txt for this sample’s licensing information
 
 Abstract:
 The OpenGLRenderer class creates and draws objects.
  Most of the code is OS independent.
 */

#import "OpenGLRenderer.h"

@interface OpenGLRenderer ()
{
    int _viewWidth;
    int _viewHeight;
}
@end

@implementation OpenGLRenderer


- (void) resizeWithWidth:(int)width AndHeight:(int)height
{
	_viewWidth = width;
	_viewHeight = height;
}

- (void) render
{

}




- (void) dealloc
{

}

@end
