/*
 *  ______                   __  __              __
 * /\  _  \           __    /\ \/\ \            /\ \__
 * \ \ \L\ \  __  __ /\_\   \_\ \ \ \____    ___\ \ ,_\   ____
 *  \ \  __ \/\ \/\ \\/\ \  /'_` \ \ '__`\  / __`\ \ \/  /',__\
 *   \ \ \/\ \ \ \_/ |\ \ \/\ \L\ \ \ \L\ \/\ \L\ \ \ \_/\__, `\
 *    \ \_\ \_\ \___/  \ \_\ \___,_\ \_,__/\ \____/\ \__\/\____/
 *     \/_/\/_/\/__/    \/_/\/__,_ /\/___/  \/___/  \/__/\/___/
 * @copyright Copyright 2017 Avidbots Corp.
 * @name   spawn_model_tool.cpp
 * @brief  Rviz compatible tool for spawning flatland model
 * @author Joseph Duchesne
 * @author Mike Brousseau
 *
 * Software License Agreement (BSD License)
 *
 *  Copyright (c) 2017, Avidbots Corp.
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *   * Neither the name of the Avidbots Corp. nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 *  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 *  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

#include <OGRE/OgreEntity.h>
#include <OGRE/OgreMaterial.h>
#include <OGRE/OgreSubEntity.h>

#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QString>
#include <QVBoxLayout>
#include <QWidget>

#include <boost/filesystem.hpp>

#include <flatland_msgs/SpawnModel.h>

#include "flatland_viz/load_model_dialog.h"
#include "flatland_viz/spawn_model_tool.h"
// #include "load_model_dialog.h"
// #include "spawn_model_tool.h"

class DialogOptionsWidget;

namespace flatland_viz {
QString SpawnModelTool::path_to_model_file;
QString SpawnModelTool::model_name;

// Set the "shortcut_key_" member variable defined in the
// superclass to declare which key will activate the tool.
SpawnModelTool::SpawnModelTool() : moving_model_node_(NULL) {
  shortcut_key_ = 'm';
}

// The destructor destroys the Ogre scene nodes for the models so they
// disappear from the 3D scene.  The destructor for a Tool subclass is
// only called when the tool is removed from the toolbar with the "-"
// button.
SpawnModelTool::~SpawnModelTool() {
  scene_manager_->destroySceneNode(arrow_->getSceneNode());
  scene_manager_->destroySceneNode(moving_model_node_);
}

// onInitialize() is called by the superclass after scene_manager_ and
// context_ are set.  It should be called only once per instantiation.
// This is where most one-time initialization work should be done.
// onInitialize() is called during initial instantiation of the tool
// object.  At this point the tool has not been activated yet, so any
// scene objects created should be invisible or disconnected from the
// scene at this point.
//
// In this case we load a mesh object with the shape and appearance of
// the model, create an Ogre::SceneNode for the moving model, and then
// set it invisible.
void SpawnModelTool::onInitialize() {
  // hide 3d model until the user clicks the span model tool button
  model_state = m_hidden;

  // make an arrow to show axis of rotation
  arrow_ = new rviz::Arrow(scene_manager_, NULL, 2.0f, 0.2f, 0.5f, 0.35f);
  arrow_->setColor(0.0f, 0.0f, 1.0f, 1.0f);  // blue
  arrow_->getSceneNode()->setVisible(
      false);  // will only be visible during rotation phase

  // set arrow to point up (along z)
  Ogre::Quaternion orientation(Ogre::Radian(M_PI), Ogre::Vector3(1, 0, 0));
  arrow_->setOrientation(orientation);

  // full path to the model file
  model_resource_ = "package://flatland_viz/media/simple.dae";

  // load the 3d model
  if (rviz::loadMeshFromResource(model_resource_).isNull()) {
    ROS_ERROR("SpawnModelTool: failed to load model resource '%s'.",
              model_resource_.c_str());
    return;
  }

  // create an Ogre child scene node
  moving_model_node_ =
      scene_manager_->getRootSceneNode()->createChildSceneNode();

  // create an Ogre entity
  Ogre::Entity *entity = scene_manager_->createEntity(model_resource_);

  // attach the object to the entity
  moving_model_node_->attachObject(entity);
  moving_model_node_->setVisible(false);

  SetMovingModelColor(Qt::green);
}

// Activate() is called when the tool is started by the user, either
// by clicking on its button in the toolbar or by pressing its hotkey.
//
// First we set the moving model node to be visible, then we create an
// rviz::VectorProperty to show the user the position of the model.
// Unlike rviz::Display, rviz::Tool is not a subclass of
// rviz::Property, so when we want to add a tool property we need to
// get the parent container with getPropertyContainer() and add it to
// that.
//
// We wouldn't have to set current_model_property_ to be read-only, but
// if it were writable the model should really change position when the
// user edits the property.  This is a fine idea, and is possible, but
// is left as an exercise for the reader.
void SpawnModelTool::activate() {
  ROS_INFO_STREAM("SpawnModelTool::activate ");

  LoadModelDialog *model_dialog = new LoadModelDialog(NULL, this);
  model_dialog->setModal(true);
  model_dialog->show();
}

void SpawnModelTool::BeginPlacement() {
  ROS_INFO_STREAM("SpawnModelTool::BeginPlacement");
  model_state = m_dragging;

  if (moving_model_node_) {
    moving_model_node_->setVisible(true);
  }
}

// deactivate() is called when the tool is being turned off because
// another tool has been chosen.
//
// We make the moving model invisible, then delete the current model
// property.  Deleting a property also removes it from its parent
// property, so that doesn't need to be done in a separate step.  If
// we didn't delete it here, it would stay in the list of models when
// we switch to another tool.
void SpawnModelTool::deactivate() {
  if (moving_model_node_) {
    moving_model_node_->setVisible(false);
  }
}

void SpawnModelTool::SpawnModelInFlatland() {
  ROS_INFO_STREAM(
      "SpawnModelTool::SpawnModelInFlatland name:" << model_name.toStdString());
  flatland_msgs::SpawnModel srv;

  // model names can not have embeded period char
  model_name = model_name.replace(".", "_", Qt::CaseSensitive);

  // fill in the service request
  srv.request.name = model_name.toStdString();
  srv.request.ns = model_name.toStdString();
  srv.request.yaml_path = path_to_model_file.toStdString();
  srv.request.pose.x = intersection[0];
  srv.request.pose.y = intersection[1];
  srv.request.pose.theta = initial_angle + M_PI / 2.0f;

  client = nh.serviceClient<flatland_msgs::SpawnModel>("spawn_model");

  // make ros service call
  bool client_is_running = client.call(srv);

  if (!client_is_running) {
    QMessageBox msgBox;
    msgBox.setText("Error: You must have a client running.");
    msgBox.exec();
  } else {
    if (!srv.response.success) {
      QMessageBox msgBox;
      msgBox.setText(srv.response.message.c_str());
      msgBox.exec();
    }
  }
}

void SpawnModelTool::SetMovingModelColor(QColor c) {
  ROS_INFO_STREAM("SpawnModelTool::SetMovingModelColor");

  Ogre::Entity *m_pEntity =
      static_cast<Ogre::Entity *>(moving_model_node_->getAttachedObject(0));
  const Ogre::MaterialPtr m_pMat = m_pEntity->getSubEntity(0)->getMaterial();
  m_pMat->getTechnique(0)->getPass(0)->setAmbient(1, 0, 0);
  m_pMat->getTechnique(0)->getPass(0)->setDiffuse(c.redF(), c.greenF(),
                                                  c.blueF(), 0);
}

// processMouseEvent() is sort of the main function of a Tool, because
// mouse interactions are the point of Tools.
//
// We use the utility function rviz::getPointOnPlaneFromWindowXY() to
// see where on the ground plane the user's mouse is pointing, then
// move the moving model to that point and update the VectorProperty.
//
// If this mouse event was a left button press, we want to save the
// current model location.  Therefore we make a new model at the same
// place and drop the pointer to the VectorProperty.  Dropping the
// pointer means when the tool is deactivated the VectorProperty won't
// be deleted, which is what we want.
int SpawnModelTool::processMouseEvent(rviz::ViewportMouseEvent &event) {
  if (!moving_model_node_) {
    return Render;
  }

  Ogre::Vector3 intersection2;
  Ogre::Plane ground_plane(Ogre::Vector3::UNIT_Z, 0.0f);

  if (model_state == m_dragging) {
    if (rviz::getPointOnPlaneFromWindowXY(event.viewport, ground_plane, event.x,
                                          event.y, intersection)) {
      moving_model_node_->setVisible(true);
      moving_model_node_->setPosition(intersection);

      if (event.leftDown()) {
        model_state = m_rotating;
        arrow_->getSceneNode()->setVisible(true);
        arrow_->setPosition(intersection);

        return Render;
      }
    } else {
      moving_model_node_->setVisible(
          false);  // If the mouse is not pointing at the
                   // ground plane, don't show the model.
    }
  }
  if (model_state == m_rotating) {  // model_state is m_rotating

    if (rviz::getPointOnPlaneFromWindowXY(event.viewport, ground_plane, event.x,
                                          event.y, intersection2)) {
      if (event.leftDown()) {
        model_state = m_hidden;
        arrow_->getSceneNode()->setVisible(false);

        // change Vector3 from x,y,z to x,y,angle
        // z is zero since we are intersecting with the ground
        intersection[2] = initial_angle;

        SpawnModelInFlatland();

        return Render | Finished;
      }
      moving_model_node_->setVisible(true);
      moving_model_node_->setPosition(intersection);
      Ogre::Vector3 dir = intersection2 - intersection;
      if (fabs(dir.y) > .001) {
        initial_angle = -1.0 * atan2(dir.x, dir.y);
      } else {
        initial_angle = -90.0 * M_PI / 180.0;
      }
      Ogre::Quaternion orientation(Ogre::Radian(initial_angle),
                                   Ogre::Vector3(0, 0, 1));
      moving_model_node_->setOrientation(orientation);
    }
  }
  return Render;
}

void SpawnModelTool::SavePath(QString p) { path_to_model_file = p; }

void SpawnModelTool::SaveName(QString n) { model_name = n; }

// At the end of every plugin class implementation, we end the
// namespace and then tell pluginlib about the class.  It is important
// to do this in global scope, outside our package's namespace.

}  // end namespace flatland_viz

#include <pluginlib/class_list_macros.h>
PLUGINLIB_EXPORT_CLASS(flatland_viz::SpawnModelTool, rviz::Tool)
