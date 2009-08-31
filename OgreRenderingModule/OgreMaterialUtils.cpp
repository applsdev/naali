// For conditions of distribution and use, see copyright notice in license.txt
/// @file OgreMaterialUtils.cpp
/// Contains some common methods for 

#include "StableHeaders.h"
#include "OgreMaterialUtils.h"
#include "OgreMaterialResource.h"
#include "OgreRenderingModule.h"

namespace OgreRenderer
{
    std::string BaseMaterials[] = {
        "LitTextured", // normal
        "UnlitTextured", // normal fullbright
        "LitTexturedAdd", // additive
        "UnlitTexturedAdd", // additive fullbright
        "LitTexturedSoftAlpha", // forced soft alpha
        "UnlitTexturedSoftAlpha", // forced soft alpha fullbright
        "LitTexturedVCol", // vertexcolor
        "UnlitTexturedVCol", // vertexcolor fullbright
        "LitTexturedSoftAlphaVCol", // vertexcolor forced soft alpha
        "UnlitTexturedSoftAlphaVCol" // vertexcolor forced soft alpha fullbright
    };
    
    std::string AlphaBaseMaterials[] = {
        "LitTexturedSoftAlpha", // soft alpha
        "UnlitTexturedSoftAlpha", // soft alpha fullbright
        "LitTexturedAdd", // additive
        "UnlitTexturedAdd", // additive fullbright
        "LitTexturedSoftAlpha", // forced soft alpha
        "UnlitTexturedSoftAlpha", // forced soft alpha fullbright
        "LitTexturedSoftAlphaVCol", // vertexcolor soft alpha
        "UnlitTexturedSoftAlphaVCol", // vertexcolor soft alpha fullbright
        "LitTexturedSoftAlphaVCol", // vertexcolor forced soft alpha
        "UnlitTexturedSoftAlphaVCol" // vertexcolor forced soft alpha fullbright
    };
    
    std::string MaterialSuffix[] = {
        "", // normal
        "fb", // normal fullbright
        "add", // additive
        "fbadd", // additive fullbright
        "alpha", // forced alpha
        "fbalpha", // forced alpha fullbright
        "vcol", // vertex color
        "fbvcol", // vertex color fullbright
        "vcolalpha", // vertex color alpha
        "fbvcolalpha" // vertex color alpha fullbright
    };

    bool IsMaterialSuffixValid(const std::string& suffix)
    {
        for (Core::uint i = 0; i < MAX_MATERIAL_VARIATIONS; ++i)
        {
            if (suffix == MaterialSuffix[i])
                return true;
        }
        
        return false;
    }
    
    std::string GetMaterialSuffix(Core::uint variation)
    {
        if (variation >= MAX_MATERIAL_VARIATIONS)
        {
            OgreRenderingModule::LogWarning("Requested suffix for non-existing material variation " + Core::ToString<Core::uint>(variation));
            variation = 0;
        }
        
        return MaterialSuffix[variation];
    }

    Ogre::MaterialPtr GetOrCreateLitTexturedMaterial(const char *materialName)
    {
        const char baseMaterialName[] = "LitTextured";

        Ogre::MaterialManager &mm = Ogre::MaterialManager::getSingleton();
        Ogre::MaterialPtr material = mm.getByName(materialName);

        if (!material.get())
        {
            Ogre::MaterialPtr baseMaterial = mm.getByName(baseMaterialName);
            material = baseMaterial->clone(materialName);
        }

        assert(material.get());
        return material;
    }

    Ogre::MaterialPtr GetOrCreateUnlitTexturedMaterial(const char *materialName)
    {
        const char baseMaterialName[] = "UnlitTextured";

        Ogre::MaterialManager &mm = Ogre::MaterialManager::getSingleton();
        Ogre::MaterialPtr material = mm.getByName(materialName);

        if (!material.get())
        {
            Ogre::MaterialPtr baseMaterial = mm.getByName(baseMaterialName);
            material = baseMaterial->clone(materialName);
        }

        assert(material.get());
        return material;
    }

    void CreateLegacyMaterials(const std::string& texture_name, bool update)
    {
        Ogre::TextureManager &tm = Ogre::TextureManager::getSingleton();
        Ogre::MaterialManager &mm = Ogre::MaterialManager::getSingleton();
        
        Ogre::TexturePtr tex = tm.getByName(texture_name);
        bool has_alpha = false;
        if (!tex.isNull())
        {
            if (Ogre::PixelUtil::hasAlpha(tex->getFormat()))
                has_alpha = true;
        }
        
        // Early out: if texture does not yet exist and materials have already been created once
        if (((tex.isNull()) || (!update)) && (!mm.getByName(texture_name).isNull()))
            return;
        
        for (Core::uint i = 0; i < MAX_MATERIAL_VARIATIONS; ++i)
        {
            const std::string& base_material_name = BaseMaterials[i];
            const std::string& alpha_base_material_name = AlphaBaseMaterials[i];
            
            std::string material_name = texture_name + MaterialSuffix[i];
            Ogre::MaterialPtr material = mm.getByName(material_name);

            if (!material.get())
            {
                material = mm.create(material_name, Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME);
                assert(material.get());
            }
            
            Ogre::MaterialPtr base_material;
            if (!has_alpha)
                base_material = mm.getByName(base_material_name);
            else
                base_material = mm.getByName(alpha_base_material_name);
            if (!base_material.get())
            {
                OgreRenderingModule::LogError("Could not find " + MaterialSuffix[i] + " base material for " + texture_name);
                return;
            }

            base_material->copyDetailsTo(material);
            SetTextureUnitOnMaterial(material, texture_name, 0);
        }
    }

    void SetTextureUnitOnMaterial(Ogre::MaterialPtr material, const std::string& texture_name, Core::uint index)
    {
        Ogre::TextureManager &tm = Ogre::TextureManager::getSingleton();
        Ogre::TexturePtr tex = tm.getByName(texture_name);
        
        Ogre::Material::TechniqueIterator iter = material->getTechniqueIterator();
        while(iter.hasMoreElements())
        {
            Ogre::Technique *tech = iter.getNext();
            assert(tech);
            Ogre::Technique::PassIterator passIter = tech->getPassIterator();
            while(passIter.hasMoreElements())
            {
                Ogre::Pass *pass = passIter.getNext();
                
                Ogre::Pass::TextureUnitStateIterator texIter = pass->getTextureUnitStateIterator();
                Core::uint cmp_index = 0;
                
                while(texIter.hasMoreElements())
                {
                    Ogre::TextureUnitState *texUnit = texIter.getNext();
                    if (index == cmp_index) 
                    {
                        if (tex.get())
                            texUnit->setTextureName(texture_name);
                        else
                            texUnit->setTextureName("TextureMissing.png");
                    }
                    cmp_index++;
                }
            }
        }
    }
    
    Foundation::ResourcePtr OGRE_MODULE_API CreateResourceFromMaterial(Ogre::MaterialPtr material)
    {
        assert(!material.isNull());
        OgreMaterialResource* res = new OgreMaterialResource(material->getName());
        res->SetMaterial(material);
        Foundation::ResourcePtr res_ptr(res);
        return res_ptr;
    }
}
