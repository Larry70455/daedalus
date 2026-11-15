add_library(HLEGraphics_PSP OBJECT 
                                    Blender.cpp 
                                    BlendModes.cpp 
                                    ColourAdjuster.cpp 
                                    ConvertVertices.S 
                                    DisplayListDebugger.cpp 
                                    GraphicsPluginPSP.cpp 
                                    RendererPSP.cpp 
                                    TnLVFPU.S 
                                    TransformWithColour.S
                                    TransformWithLighting.S
                                    VectorClipping.S

                                    Combiner/BlendConstant.cpp 
                                    Combiner/CombinerExpression.cpp 
                                    Combiner/CombinerTree.cpp 
                                    Combiner/RenderSettings.cpp
            )

if(DAEDALUS_PROFILE_HLEGRAPHICS_PSP)
    target_compile_options(HLEGraphics_PSP PRIVATE -pg)
endif(DAEDALUS_PROFILE_HLEGRAPHICS_PSP)

list(APPEND daed_libs "HLEGraphics_PSP")
set(daed_libs ${daed_libs} PARENT_SCOPE)

# The PSP has a 16 KB instruction cache, and its hardware profiler showed the
# display list code missing it constantly (about 52000 misses per frame in a
# Pokemon Stadium 2 battle). -Os makes this code about 40 percent smaller.
if(DAEDALUS_PSP AND DAEDALUS_PSP_GRAPHICS_OS)
    target_compile_options(HLEGraphics_PSP PRIVATE -Os)
endif()
