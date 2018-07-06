#ifndef WALLENGINE_OBJECT3D_H
#define WALLENGINE_OBJECT3D_H

namespace wp
{
    class object3d
    {
    public:
        enum Type
        {
            Type_Image = 0,
            Type_Model = 1,
            Type_Particle = 2,
            Type_None = 3
        };

        object3d (Type type);

        template <class T> T* as();
        template <class T> bool is();

    private:
        Type m_type;
    };
};

#endif //WALLENGINE_OBJECT3D_H
