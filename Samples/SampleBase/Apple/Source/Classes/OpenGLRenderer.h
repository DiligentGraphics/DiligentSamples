 /*
 Copyright (C) 2015 Apple Inc. All Rights Reserved.
 See LICENSE.txt for this sample’s licensing information
 
 Abstract:
 The OpenGLRenderer class creates and draws objects.
  Most of the code is OS independent.
 */
#import <Foundation/Foundation.h>

@interface OpenGLRenderer : NSObject 

- (void) resizeWithWidth:(int)width AndHeight:(int)height;
- (void) render;
- (void) dealloc;

@end
