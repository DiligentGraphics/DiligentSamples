
#include "structures.fxh"

[shader("miss")]
void main(inout ShadowRayPayload payload)
{
	// Set 0 on hit and 1 otherwise.
	payload.Shading = 1.0;
}
