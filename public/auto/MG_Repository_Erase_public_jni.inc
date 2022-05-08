
JNIEXPORT jobject JNICALL Java_runtime_Bones_nativeRepository_Erase
    (JNIEnv* jenv, jobject obj, jobjectArray j_objects)
{
    const char* argName = nullptr;
    try
    {
        argName = "objects (input #1)";
        const Vector_<Handle_<Storable_>> objects = JNI::ToHandleBaseVector(jenv, j_objects);
        argName = nullptr;

		int num_erased;
        Repository_Erase(objects, &num_erased);
        static const jclass THE_CLASS = jenv->FindClass("runtime.Bones/Repository_Erase_Outputs");
        REQUIRE(THE_CLASS, "Can't find output class");
        static const jmethodID THE_CONSTRUCTOR = jenv->GetMethodID(THE_CLASS, "<init>", "(I)V");
        REQUIRE(THE_CONSTRUCTOR, "Can't find output initializer");
		auto num_erased_ret = JNI::CopyOut(jenv, num_erased);
        return jenv->NewObject(THE_CLASS, THE_CONSTRUCTOR, num_erased_ret);
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
