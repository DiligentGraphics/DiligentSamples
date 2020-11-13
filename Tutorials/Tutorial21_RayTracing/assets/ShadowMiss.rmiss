
#include "structures.fxh"

[shader("miss")]
void main(inout ShadowRayPayload payload)
{
	// Set 0 on hit and 1 without hit.
	payload.Shading = 1.0;
}
