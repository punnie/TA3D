/*  TA3D, a remake of Total Annihilation
    Copyright (C) 2005  Roland BROCHARD

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA*/

/*-----------------------------------------------------------------------------\
|                                      3ds.cpp                                 |
|        This module is a loader for the 3DS file format, it loads a model     |
|  into an MODEL object that can be used by the 3dmeditor.                     |
\-----------------------------------------------------------------------------*/

#include "stdafx.h"
#include "TA3D_NameSpace.h"
#define TA3D_BASIC_ENGINE
#include "ta3dbase.h"		// Moteur
#include "misc/paths.h"
#include "obj.h"
#include "3dmeditor.h"
#include <vector>
#include "misc/vector.h"


class Material
{
public:
    String  name;
    String  textureName;
    
    Material() : name(), textureName()  {}
    Material(int a) : name(), textureName() {}
};

    // fill the OBJECT with gathered data
void finalize_object( OBJECT *cur, std::vector<int> &face, std::vector<VECTOR3D> &vertex,
                      std::vector<VECTOR3D> &normal, std::vector<VECTOR2D> &tcoord, Material* mtl = NULL)
{
    cur->nb_vtx = face.size() >> 1;
    cur->nb_t_index = face.size() >> 1;
    cur->t_index = (GLushort*) malloc( sizeof( GLushort ) * cur->nb_t_index );
    cur->points = (VECTOR3D*) malloc( sizeof( VECTOR3D) * cur->nb_vtx );
    cur->N = (VECTOR3D*) malloc( sizeof( VECTOR3D) * cur->nb_vtx );
    cur->tcoord = (float*) malloc( sizeof( float ) * 2 * cur->nb_vtx );

    for (int i = 0 ; i < cur->nb_t_index ; i++)
    {
        cur->points[i] = vertex[ face[i * 2]];
        cur->tcoord[i * 2] = tcoord[ face[i * 2 + 1]].x;
        cur->tcoord[i * 2 + 1] = 1.0f - tcoord[face[i * 2 + 1]].y;
        cur->t_index[i] = i;
    }

    for (int i = 0 ; i < cur->nb_vtx; ++i)
        cur->N[i].x = cur->N[i].y = cur->N[i].z = 0.0f;
    for (int i = 0 ; i < cur->nb_vtx; i += 3)
    {
        VECTOR3D AB = cur->points[i + 1] - cur->points[i];
        VECTOR3D AC = cur->points[i + 2] - cur->points[i];
        VECTOR3D N = AB * AC;
        N.unit();
        cur->N[i] = cur->N[i] + N;
        cur->N[i+1] = cur->N[i+1] + N;
        cur->N[i+2] = cur->N[i+2] + N;
    }
    for (int i = 0 ; i < cur->nb_vtx ; i++)
        cur->N[i].unit();
    
    cur->surface.Flag = SURFACE_ADVANCED | SURFACE_LIGHTED | SURFACE_GOURAUD;
    if (mtl)
    {
        cur->surface.gltex[0] = gfx->load_texture( mtl->textureName );
        if (cur->surface.gltex[0])
        {
            cur->surface.NbTex = 1;
            cur->surface.Flag |= SURFACE_TEXTURED;
        }
    }
}

    // Load a 3D model in OBJ format
MODEL *load_obj( const String &filename, float scale )
{
    FILE *src_obj = fopen( filename.c_str(), "rb" );
    MODEL *model_obj = new MODEL;
    if( src_obj )
    {
        char buf[1024];
	    
        OBJECT *cur = &(model_obj->obj);
        bool firstObject = true;
        std::vector<VECTOR3D>  vertex;
        std::vector<VECTOR3D>  normal;
        std::vector<VECTOR2D>  tcoord;
        std::vector<int>       face;
        cHashTable<Material>  mtllib;
        Material                currentMtl;
	    
        while (fgets( buf, 1024, src_obj)) // Reads the while file
        {
            String::Vector args;
            ReadVectorString( args, buf, " " );
            if (!args.empty())
            {
                if ( args[0] == "o" && args.size() > 1)      // Creates a new object
                {
                    if (!firstObject)
                    {
                        finalize_object( cur, face, vertex, normal, tcoord, &currentMtl );
                        face.clear();
                        cur->child = new OBJECT();
                        cur = cur->child;
                    }
                    firstObject = false;
                    cur->name = strdup( args[1].c_str() );
                    printf("[obj] new object '%s'\n", args[1].c_str());
                }
                else 
                    if (args[0] == "mtllib" && args.size() > 1)        // Read given material libraries
                    {
                        for (String::Vector::const_iterator i = args.begin(); i != args.end(); ++i)
                        {
                            FILE *src_mtl = fopen( (TA3D::Paths::ExtractFilePath(filename) + *i).c_str(), "rb" );
                            if (src_mtl)
                            {
                                Material mtl;
                                while( fgets( buf, 1024, src_mtl ) )
                                {
                                    String::Vector args0;
                                    ReadVectorString( args0, buf, " " );
                                    if (args0.size() > 0)
                                    {
                                        if (args0[0] == "newmtl")
                                            mtl.name = args0[1];
                                        else if (args0[0] == "map_Kd")
                                        {
                                            mtl.textureName = TA3D::Paths::ExtractFilePath(filename) + args0[1];
                                            mtllib.insertOrUpdate( mtl.name, mtl );
                                        }
                                    }
                                }
                                fclose( src_mtl );
                            }
                        }
                    }
                    else
                        if (args[0] == "usemtl" && args.size() > 1)        // Change current material
                        {
                            if (mtllib.exists( args[1] ))
                                currentMtl = mtllib.find( args[1] );
                            else
                                currentMtl.textureName.clear();
                        }
                        else if (args[0] == "v" && args.size() > 3)  // Add a vertex to current object
                            vertex.push_back( VECTOR3D(args[1].toFloat(), args[2].toFloat(), args[3].toFloat()));
                        else if (args[0] == "vn" && args.size() > 3)  // Add a normal vector to current object
                            normal.push_back( VECTOR3D(args[1].toFloat(), args[2].toFloat(), args[3].toFloat()));
                        else if (args[0] == "vt" && args.size() > 2)  // Add a texture coordinate vector to current object
                            tcoord.push_back( VECTOR2D( args[1].toFloat(), args[2].toFloat()));
                        else if (args[0] == "f" && args.size() > 1)  // Add a face to current object
                        {
                            std::vector< int >  vertex_idx;
                            std::vector< int >  tcoord_idx;
                            std::vector< int >  normal_idx;
                            for (String::Vector::const_iterator i = args.begin(); i != args.end(); ++i)
                            {
                                String::Vector data;
                                ReadVectorString(data, *i, "/" );

                                vertex_idx.push_back( data[0].toInt32() - 1);
                                if (data.size() == 3)
                                {
                                    if (data[1].empty())
                                        tcoord_idx.push_back(-1);
                                    else
                                        tcoord_idx.push_back(data[1].toInt32() - 1);

                                    if (data[2].empty())
                                        normal_idx.push_back(-1);
                                    else
                                        normal_idx.push_back(data[2].toInt32() - 1);
                                }
                                else
                                {
                                    tcoord_idx.push_back(-1);
                                    normal_idx.push_back(-1);
                                }
                            }

                            for (unsigned int i = 2; i < vertex_idx.size(); ++i) // Make triangles (FAN method)
                            {
                                face.push_back(vertex_idx[0]);
                                face.push_back(tcoord_idx[0]);

                                face.push_back(vertex_idx[i-1]);
                                face.push_back(tcoord_idx[i-1]);

                                face.push_back(vertex_idx[i]);
                                face.push_back(tcoord_idx[i]);
                            }
                        }
            }
        }

        if (!firstObject)
            finalize_object(cur, face, vertex, normal, tcoord);

        fclose(src_obj);

        model_obj->nb_obj = model_obj->obj.set_obj_id(0);

        VECTOR3D O;
        O.x=O.y=O.z=0.0f;
        int coef=0;
        model_obj->center.x = model_obj->center.y = model_obj->center.z = 0.0f;
        model_obj->obj.compute_center(&model_obj->center,O,&coef);
        model_obj->center = (1.0f/coef) * model_obj->center;
        model_obj->size = 2.0f * model_obj->obj.compute_size_sq(model_obj->center); // On garde le carré pour les comparaisons et on prend une marge en multipliant par 2.0f
        model_obj->size2 = sqrt(0.5f*model_obj->size);
        model_obj->obj.compute_emitter();
        model_obj->compute_topbottom();
    }
    return model_obj;
}