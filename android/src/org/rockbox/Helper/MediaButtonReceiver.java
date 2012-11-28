/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id$
 *
 * Copyright (C) 2010 Thomas Martitz
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 ****************************************************************************/

package org.rockbox.Helper;

import org.rockbox.RockboxFramebuffer;
import org.rockbox.RockboxService;

import android.content.BroadcastReceiver;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.media.AudioManager;
import android.net.Uri;
import android.view.KeyEvent;

public class MediaButtonReceiver
{
    /* A note on the API being used. 2.2 introduces a new and sane API
     * for handling multimedia button presses
     * http://android-developers.blogspot.com/2010/06/allowing-applications-to-play-nicer.html
     * 
     * the old API is flawed. It doesn't have management for
     * concurrent media apps
     * 
     * if multiple media apps are running 
     * probably all of them want to respond to media keys
     * 
     * it's not clear which app wins, it depends on the
     * priority set for the IntentFilter (see below)
     * 
     * so this all might or might not work on < 2.2 */

    IMultiMediaReceiver api;

    public MediaButtonReceiver(Context c)
    {
        try {
            api = new NewApi(c);
        } catch (Throwable t) {
            /* Throwable includes Exception and the expected
             * NoClassDefFoundError */
            api = new OldApi(c);
            Logger.i("MediaButtonReceiver: Falling back to compatibility API");
        }
    }

    public void register()
    {
        api.register();
    }
    
    public void unregister()
    {
        api.unregister();
    }

    /* helper class for the manifest */
    public static class MediaReceiver extends BroadcastReceiver
    {
        private static long mLastClickTime = 0;
        private void startService(Context c, Intent baseIntent)
        {
            baseIntent.setClass(c, RockboxService.class);
            c.startService(baseIntent);
        }
        private Intent KeyIntent(Context context, int keycode)
        {
            /* 使用Intent发送keyCode到Service，达到控制播放的目的 */
            Intent intent = new Intent(Intent.ACTION_MEDIA_BUTTON, Uri.EMPTY,
                                       context, RockboxService.class);
            intent.putExtra(Intent.EXTRA_KEY_EVENT,
                            new KeyEvent(KeyEvent.ACTION_UP, keycode));
            return intent;
        }
        
        @Override
        public void onReceive(Context context, Intent intent)
        {
            boolean isActionMediaButton = Intent.ACTION_MEDIA_BUTTON.equals(intent.getAction());
            if(!isActionMediaButton) return;
            KeyEvent key = (KeyEvent)intent.getParcelableExtra(Intent.EXTRA_KEY_EVENT);
            if(key==null) return;
            boolean isActionUp = (key.getAction()==KeyEvent.ACTION_UP);
            //boolean isActionDown = (key.getAction()==KeyEvent.ACTION_DOWN);
            if(!isActionUp) return;
            //if (!isActionDown){
            int keyCode = key.getKeyCode();
            long mEventTime = key.getEventTime();
            long pressTime = mEventTime-key.getDownTime();//按键长按时长
            boolean isLongPressPre = (pressTime>500); //长按大于500ms上一曲
            RockboxService s = RockboxService.getInstance();
            boolean isHeadsetPress = false;
            if (s == null || !s.isRockboxRunning()) //先启动服务
            	startService(context, intent);
            else
            {
                switch (keyCode) {
                case KeyEvent.KEYCODE_HEADSETHOOK://播放或暂停
                case KeyEvent.KEYCODE_MEDIA_PLAY_PAUSE:
                	if (isLongPressPre) {
                        startService(context,
                	    	KeyIntent(context,KeyEvent.KEYCODE_MEDIA_PREVIOUS));
                	}else{
                		isHeadsetPress = true;
                		startService(context,
                    	    	KeyIntent(context,KeyEvent.KEYCODE_MEDIA_PLAY_PAUSE));
                	}
                    break;
                case KeyEvent.KEYCODE_MEDIA_NEXT:
                	startService(context,
                            KeyIntent(context,KeyEvent.KEYCODE_MEDIA_NEXT));
                	break;
                case KeyEvent.KEYCODE_MEDIA_PREVIOUS:
                	startService(context,
                            KeyIntent(context,KeyEvent.KEYCODE_MEDIA_PREVIOUS));
            	    break;
                default:
                	Logger.d("少侠，你的按键我不认识。");
                	break;
                }
            }
            /* single quick press: pause/resume.
             * double press: next track
             * long press: previous track */
            if (isHeadsetPress) {
            	isHeadsetPress = false; //貌似没必要
            	/* 用两次单键抵消影响 目前只能这样子了
            	 * 用mLastClickTime记录上次键击 */
            	if (mEventTime - mLastClickTime < 300 && keyCode == KeyEvent.KEYCODE_HEADSETHOOK)
                {
    				startService(context,
                            KeyIntent(context,KeyEvent.KEYCODE_MEDIA_NEXT));
    			}
            	mLastClickTime = key.getEventTime();
            }
            this.abortBroadcast(); //终止广播(免受其他播放器干扰)
        }
    }
    
    private interface IMultiMediaReceiver 
    {
        void register();
        void unregister();
    }

    private static class NewApi 
                    implements IMultiMediaReceiver, AudioManager.OnAudioFocusChangeListener
    {
        private AudioManager audio_manager;
        private ComponentName receiver_name;
        private boolean running = false;
        
        NewApi(Context c)
        {
            audio_manager = (AudioManager)c.getSystemService(Context.AUDIO_SERVICE);
            receiver_name = new ComponentName(c, MediaReceiver.class);
        }
        
        public void register()
        {
            try {
                audio_manager.registerMediaButtonEventReceiver(receiver_name);
                audio_manager.requestAudioFocus(this, AudioManager.STREAM_MUSIC, AudioManager.AUDIOFOCUS_GAIN);
                running = true;
            } catch (Exception e) {
                // Nothing
                e.printStackTrace();
            }
        }

        public void unregister()
        {
            try
            {
                audio_manager.unregisterMediaButtonEventReceiver(receiver_name);
                audio_manager.abandonAudioFocus(this);
                running = false;
            } catch (Exception e) {
                // Nothing
                e.printStackTrace();
            }
        }

        public void onAudioFocusChange(int focusChange)
        {
            Logger.d("Audio focus" + ((focusChange>0)?"gained":"lost")+
                                         ": "+ focusChange);
            if (running)
            {   /* Play nice and stop for the the other app */
                if (focusChange == AudioManager.AUDIOFOCUS_LOSS)
                    RockboxFramebuffer.buttonHandler(KeyEvent.KEYCODE_MEDIA_STOP, false);
            }
        }
        
    }

    private static class OldApi implements IMultiMediaReceiver
    {
        private static final IntentFilter filter = new IntentFilter(Intent.ACTION_MEDIA_BUTTON);
        private MediaReceiver receiver;
        private Context context;
        OldApi(Context c)
        {
            filter.setPriority(2147483647); /* 1 higher than the built-in media player ----> really? */
            receiver = new MediaReceiver();
            context = c;
        }
        
        public void register()
        {
            context.registerReceiver(receiver, filter);            
        }

        public void unregister()
        {
            context.unregisterReceiver(receiver);
        }
        
    }
}
