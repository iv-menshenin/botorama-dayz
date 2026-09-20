//! dmMath — общие геометрические примитивы (без аллокаций, для горячих циклов).
class dmMath
{
    //! Квадрат 2D-дистанции (XZ-плоскость, Y игнорируется). Для сравнений «ближе/дальше»
    //! без sqrt. Не путать с vector.DistanceSq — тот 3D (включает Y).
    static float DistSq2D(vector a, vector b)
    {
        float dx = a[0] - b[0];
        float dz = a[2] - b[2];
        return dx * dx + dz * dz;
    }
}
