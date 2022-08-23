#ifndef _RANDOM_FXH_
#define _RANDOM_FXH_

// https://www.shadertoy.com/view/Xt3cDn
//Quality hashes collection
//by nimitz 2018 (twitter: @stormoid)

//The MIT License
//Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions: The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software. THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

/*
	This is a collection of useful "high-quality" hash functions for WebGL 2 (or anything supporting uints
	and bitwise ops) returning values in the 0..1 range as 32bit floats

	I am using either the a XXhash32 (https://github.com/Cyan4973/xxHash) modified function (for low input
	count). This is a relatively fast and very high quality hash function that can be used as a basis for
	comparison.	My modified version was tested to make sure it still has quality comparable with the
	reference implementation.

	The second option is a modified version of iq's "Integer Hash III" (https://www.shadertoy.com/view/4tXyWN)
	Which I tested using ENT (http://www.fourmilab.ch/random/) and turned out to have serious quality issues.	
	I added the same XORShift finisher as XXHash to improve the characteristics and this turned out	to work
	much better than I hoped, every test I have ran suggest that the quality of the modified version is very
	high. I also implemented 1D and 3D input versions of this hash and tested to make sure the quality
	remained high.

	I did some prelinimary testing and it seems that the bit rotation line(s) of the XXHash implementations can
	be omitted without much consequence in terms of quality (for very low input count as used here).
	This would make the xxhash based method as fast as the other and the quality would likely be higher,
	further testing would be required to confirm.


	For the generation of multiple outputs dimensions (if needed) from the base hash I am using MINSTD
	with the generator parameters from: http://random.mat.sbg.ac.at/results/karl/server/node4.html
	I also tested the hash quality after the MINSTD step and there seems to be little to no loss in quality from
	that final step. One thing I did not test for is the potential correlation between the MINSTD generators,
	but I doubt	this would be an issue, let me know if turns out to be the case.

	I included the 1D and 3D input versions in the common tab as to not clutter this tab too much.


	Please report any issues, statistical or otherwise either here in the comments section
	or on twitter.


	See the bottom of this page for an example of usage with arbitrary float input.
*/


uint baseHash(uint2 p)
{
    p = 1103515245U*((p >> 1U)^(p.yx));
    uint h32 = 1103515245U*((p.x)^(p.y>>3U));
    return h32^(h32 >> 16);
}

float hash12(uint2 x)
{
    uint n = baseHash(x);
    return float(n)*(1.0/float(0xffffffffU));
}

float2 hash22(uint2 x)
{
    uint n = baseHash(x);
    uint2 rz = uint2(n, n*48271U);
    return float2((rz.xy >> 1) & uint2(0x7fffffffU, 0x7fffffffU))/float(0x7fffffff);
}

float3 hash32(uint2 x)
{
    uint n = baseHash(x);
    uint3 rz = uint3(n, n*16807U, n*48271U);
    return float3((rz >> 1) & uint3(0x7fffffffU, 0x7fffffffU, 0x7fffffffU))/float(0x7fffffff);
}

float4 hash42(uint2 x)
{
    uint n = baseHash(x);
    uint4 rz = uint4(n, n*16807U, n*48271U, n*69621U); //see: http://random.mat.sbg.ac.at/results/karl/server/node4.html
    return float4((rz >> 1) & uint4(0x7fffffffU, 0x7fffffffU, 0x7fffffffU, 0x7fffffffU))/float(0x7fffffff);
}

#endif // _RANDOM_FXH_
