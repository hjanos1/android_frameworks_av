/*
 * Copyright (C) 2012 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdint.h>
#include <sys/types.h>

#include <media/IRemoteDisplay.h>

namespace android {

enum {
    DISCONNECT = IBinder::FIRST_CALL_TRANSACTION,
};

class BpRemoteDisplay: public BpInterface<IRemoteDisplay>
{
public:
    BpRemoteDisplay(const sp<IBinder>& impl)
        : BpInterface<IRemoteDisplay>(impl)
    {
    }

    status_t disconnect()
    {
        Parcel data, reply;
        data.writeInterfaceToken(IRemoteDisplay::getInterfaceDescriptor());
        remote()->transact(DISCONNECT, data, &reply);
        return reply.readInt32();
    }
};

IMPLEMENT_META_INTERFACE(RemoteDisplay, "android.media.IRemoteDisplay");

// ----------------------------------------------------------------------

status_t BnRemoteDisplay::onTransact(
    uint32_t code, const Parcel& data, Parcel* reply, uint32_t flags)
{
    switch (code) {
        case DISCONNECT: {
            CHECK_INTERFACE(IRemoteDisplay, data, reply);
            reply->writeInt32(disconnect());
            return NO_ERROR;
        }
        default:
            return BBinder::onTransact(code, data, reply, flags);
    }
}

}; // namespace android