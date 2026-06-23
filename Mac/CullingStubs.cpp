// CullingStubs.cpp — stub for CullingSchemes.cpp symbols not yet ported to wx
// isBlockCulled and related state will be fully ported when the Culling Scheme
// dialog is implemented; for now all blocks are visible.
#include "stdafx.h"

bool gIsCulledByIndex[1200] = {};
bool gAnyCulled = false;

bool isBlockCulled(int /*type*/, int /*dataVal*/) { return false; }
void applyCullingScheme(void* /*scheme*/) {}
void seedDefaultCulled() {}
