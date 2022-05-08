
JNIEXPORT jobject JNICALL Java_runtime_Base_nativeInterp2_Get
    (JNIEnv* jenv, jobject obj, jobject j_f, jdoubleArray j_x, jdoubleArray j_y)
{
    const char* argName = nullptr;
    try
    {
        argName = "f (input #1)";
        const Handle_<Interp2_> f = JNI::ToHandle<Interp2_>(jenv, j_f);
        argName = "x (input #2)";
        const Vector_<double> x = JNI::ToDoubleVector(jenv, j_x);
        argName = "y (input #3)";
        const Vector_<double> y = JNI::ToDoubleVector(jenv, j_y);
        argName = nullptr;

		Matrix_<double> z;
        Interp2_Get(f, x, y, &z);
        static const jclass THE_CLASS = jenv->FindClass("runtime.Base/Interp2_Get_Outputs");
        REQUIRE(THE_CLASS, "Can't find output class");
        static const jmethodID THE_CONSTRUCTOR = jenv->GetMethodID(THE_CLASS, "<init>", "([[D)))V");
        REQUIRE(THE_CONSTRUCTOR, "Can't find output initializer");
		auto z_ret = JNI::CopyOut(jenv, z);
        return jenv->NewObject(THE_CLASS, THE_CONSTRUCTOR, z_ret);
    }
    catch (std::exception& e)
    {
        return JNI::Error(jenv, e.what(), argName);
    }
    catch (...)
    {
        return JNI::Error(jenv, "Unknown error", argName);
    }
}
