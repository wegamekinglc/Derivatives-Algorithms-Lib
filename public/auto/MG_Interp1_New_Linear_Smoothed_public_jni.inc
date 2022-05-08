
JNIEXPORT jobject JNICALL Java_runtime_Bones_nativeInterp1_New_Linear_Smoothed
    (JNIEnv* jenv, jobject obj, jstring j_name, jdoubleArray j_x, jdoubleArray j_y, jdouble j_smoothing, jdoubleArray j_fit_weights)
{
    const char* argName = nullptr;
    try
    {
        argName = "name (input #1)";
        const String_ name = JNI::ToString(jenv, j_name);
        argName = "x (input #2)";
        const Vector_<double> x = JNI::ToDoubleVector(jenv, j_x);
        argName = "y (input #3)";
        const Vector_<double> y = JNI::ToDoubleVector(jenv, j_y);
        argName = "smoothing (input #4)";
        const double smoothing = JNI::ToDouble(jenv, j_smoothing);
        argName = "fit_weights (input #5)";
        const Vector_<double> fit_weights = JNI::ToDoubleVector(jenv, j_fit_weights, true);
        argName = nullptr;

		Handle_<Interp1_> f;
        Interp1_New_Linear_Smoothed(name, x, y, smoothing, fit_weights, &f);
        static const jclass THE_CLASS = jenv->FindClass("runtime.Bones/Interp1_New_Linear_Smoothed_Outputs");
        REQUIRE(THE_CLASS, "Can't find output class");
        static const jmethodID THE_CONSTRUCTOR = jenv->GetMethodID(THE_CLASS, "<init>", "(LDA/Handle;)V");
        REQUIRE(THE_CONSTRUCTOR, "Can't find output initializer");
		auto f_ret = JNI::CopyOut(jenv, f);
        return jenv->NewObject(THE_CLASS, THE_CONSTRUCTOR, f_ret);
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
