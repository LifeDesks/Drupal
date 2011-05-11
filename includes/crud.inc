<?php
/* $Id: crud.inc,v 1.16 2008/01/19 07:05:50 adrian Exp $ */

/* --- NODE / CONTENT --- */

/**
 * Given a content_type array, enable the new content type
 */
function install_add_content_type($content_type) {
  $obj = (object)_node_type_set_defaults($content_type);
  node_type_save($obj);
}

/* --- USER --- */

/**
 * Add a user
 */
function install_add_user($username, $password, $email, $roles = array(), $status = 1) {
  $role_assignments = array();

  // Translate array of names into rid => name that user_save() is expecting.
  if (!empty($roles)) {
    foreach ($roles as $name) {
      $rid = install_get_rid($name);
      $role_assignments[$rid] = $name;
    }
  }

  user_save(
    new stdClass(),
    array(
      'name' => $username, 
      'pass' => $password,
      'mail' => $email,
      'roles' => $role_assignments,
      'status' => $status
    )
  );	
}

/* --- CONTACT --- */

/**
 * Add a new contact category, including recipients and so on
 */
function install_contact_add_category($category, $recipients, $reply = '', $weight = 0, $selected = 0) {
  $cid = db_next_id("{contact}_cid");
  db_query("INSERT INTO {contact} (cid, category, recipients, reply, weight, selected) VALUES (%d, category = '%s', recipients = '%s', reply = '%s', weight = %d, selected = %d)", $cid, $category, $recipients, $reply, $weight, $selected);
}

/* --- PROFILE FUNCTIONS --- */

/**
 * Given a funky array of profile fields, create them.
 */
function install_profile_field_add($data) {
	if (!is_array($data) || !isset($data['title']) || !isset($data['name'])) {
	  return false;
	}
	$data['fid'] = db_next_id("{profile_fields}_fid");
	$data['category'] = ($data['category']) ? $data['category'] : '';
	$data['type'] = ($data['type']) ? $data['type'] : 'textfield';
	$data['required'] = ($data['required']) ? $data['required'] : '0';
	$data['register'] = ($data['register']) ? $data['register'] : '0';
	$data['visibility'] = ($data['visibility']) ? $data['visibility'] : '0';
	$data['explanation'] = ($data['explanation']) ? $data['explanation'] : '';
	
	$fields = array_keys($data);
	
	// Prepare the query:
	foreach ($data as $key => $value) {
	  if (in_array((string) $key, $fields)) {
	    $k[] = db_escape_string($key);
	    $v[] = $value;
	    $s[] = "'%s'";
	  }
	}
	db_query("INSERT INTO {profile_fields} (". implode(", ", $k) .") VALUES (". implode(", ", $s) .")", $v);
	
	return $data['fid'];
}

/* --- ROLE --- */

/**
 * Add a role to the roles table.
 */
function install_add_role($name) {
  $rid = db_result(db_query("SELECT rid FROM {role} WHERE name = '%s' LIMIT 1", $name));
  if(!$rid) {
    db_query("INSERT INTO {role} (name) VALUES ('%s')", $name);
    return install_get_rid($name);
  }
  return $rid;
}

/**
 * Get the role id for the role name
 */
function install_get_rid($name) {
  return db_result(db_query("SELECT rid FROM {role} WHERE name ='%s' LIMIT 1", $name));
}

/**
 * Set the permission for a certain role
 */
function install_set_permissions($rid, $perms) {
  db_query('DELETE FROM {permission} WHERE rid = %d', $rid);
  db_query("INSERT INTO {permission} (rid, perm) VALUES (%d, '%s')", $rid, implode(', ', $perms));
}

/* --- MENU --- */

/**
 * Create a new top-level menu.
 *
 * @param $title
 *   The title of the menu as seen by users.
 * @param $menu_name
 *   Optional. System menu name.
 * @param $desc
 *   Optional. The description of the menu.
 * 
 * @return 
 *   Returns either FALSE if there was an error, or the system menu-name as stored in the db.
 */
function install_menu_create_menu($title, $menu_name = '', $desc = '') {
  $return = FALSE;

  $title            = $title;
  $description      = $desc;
  $mn               = (empty($menu_name)) ? $title : $menu_name;
  $menu_name        = str_replace(array(' ', '_'), '-', strtolower($mn));
  $menu_name_custom = 'menu-' . $menu_name;

  // Check the db for an existing menu by the same name.
  $menu = db_result(db_query_range("SELECT menu_name FROM {menu_custom} WHERE menu_name = '%s' OR menu_name = '%s'", $menu_name, $menu_name_custom, 0, 1));

  // If a menu does not exists then we create it.
  // Appending 'menu-' to the beginning is standard for the menu module
  // so we do the same thing.
  if (!$menu) {
    $menu = $menu_name_custom;
    
    // Set up data for menu_link table.
    $path = 'admin/build/menu-customize/';
    $link = array(
      'link_title'  => $title,
      'link_path'   => $path . $menu,
      'router_path' => $path . '%',
      'module'      => 'menu',
      'plid'        => db_result(db_query("SELECT mlid FROM {menu_links} WHERE link_path = '%s' AND module = '%s'", 'admin/build/menu', 'system')),
    );
    // Save link to menu_link table.
    menu_link_save($link);
    
    // Save menu to menu_custom table.
    db_query("INSERT INTO {menu_custom} (menu_name, title, description) VALUES ('%s', '%s', '%s')", $menu_name_custom, $title, $description);
  }
  
  $return = $menu;

  return $return;
}

/**
 * Set an existing menu ID to a new parent.
 *
 * @param $mlid
 *   The menu link id that is to be reassigned.
 * @param $plid
 *   The parent link id to assign to the mlid value.
 * @param $weight
 *   The weight desired to set this menu item to.
 */
function install_menu_set_menu($mlid, $plid = NULL, $weight = NULL) {
  $return = FALSE;
  $update = FALSE;

  $result = db_query("SELECT * FROM {menu_links} WHERE mlid = %d", $mlid);
  while ($row = db_fetch_array($result)) {
    $curr_menu = $row;
  }

  // See if we need to update the parent ID.
  if (!is_null($plid) && $plid !== $curr_menu['plid']) {
      drupal_set_message($curr_menu['plid']);
      $curr_menu['plid'] = $plid;
      $update = TRUE;
      $weight = (!is_null($weight)) ? $weight : 0 ;
  }

  // See if we need to update the weight
  if (!is_null($weight) && $curr_menu['weight']) {
    $curr_menu['weight'] = $weight;
    $update = TRUE;
  }

  // Update only if needed.
  if ($update) {
    $return = menu_link_save($curr_menu);
  }

  return $return;
}

/**
 * Create a new menu item.
 *
 * TODO: inspect the $path argument. The menu system will only register menu
 * items whose path is already in the DB.
 *
 * @param  $path
 *   Path of the new menu item
 * @param  $title
 *   Title of the menu item (visible label for menu).
 * @param  $description
 *   Description of the menu item (tooltip).
 * @param  $menu
 *   Menu to associate this link with.
 * @param  $plid
 *   Parent link ID.
 * @param  $weight
 *   Weight for positioning.
 * @param  $module
 *   Optional. Specify a specific module responsible for this menu item.
 * 
 * @return  integer
 *   The database MLID of the new menu item.
 */
function install_menu_create_menu_item($path, $title, $description = '', $menu = 'navigation', $plid = 0, $weight = 0, $module = 'menu', $hidden = 0, $has_children = 0, $expanded = 0, $customized = 0, $updated = 0) {
  // If menu exists then we should insure that the menu has been created.
  $menu = install_menu_create_menu($menu);

  $option_arr = array();
  // If a description is present.
  if (!empty($description)) {
    $option_arr['attributes'] = array('title' => $description);
  }

  $link = array(
    'menu_name'     => $menu,
    'weight'        => $weight,
    'link_title'    => $title,
    'options'       => $option_arr,
    'module'        => $module,
    'link_path'     => drupal_get_normal_path($path),
    'hidden'        => $hidden,
    'has_children'  => $has_children,
    'expanded'      => $expanded,
    'customized'    => $customized,
    'updated'       => $updated,
  );

  // Since menu_save_link will do a query if any plid is passed.
  // Omit it if this is a top level link.
  if ($plid) {
    $link['plid'] = $plid;
  }

  $return = menu_link_save($link);

  // If no $return link, then inform the installer that this path in not valid.
  if (!$return) {
    drupal_set_message('The menu path: <em>"' . $path . '"</em> is not a valid path. Please insure that the necessary modules are set as dependents.');
  }

  return $return;
}

/**
 * Creates a batch of menu items.
 *
 * @param $items
 *   An array containing menu items (as arrays).
 * @param $plid
 *   Parent Link ID for which menu the items are being added.
 */
function install_menu_create_menu_items($items, $plid=0) {
  foreach ($items as $id => $item) {
    // Format the data to send.
    $path = ($item['link_path']) ? $item['link_path'] : FALSE;
    $title = ($item['link_title']) ? $item['link_title'] : FALSE;
    $desc  = ($item['description']) ? $item['description'] : '';
    $menu  = ($item['menu_name']) ? $item['menu_name'] : 'navigation';
    $plid  = ($plid) ? $plid : 0;
    $weight = ($item['weight']) ? $item['weight'] : 0;
    $module = ($item['module']) ? $item['module'] : 'menu';
    $children = (isset($item['has_children'])) ? $item['has_children'] : FALSE;

    // Create the menu link.
    $mlid = install_menu_create_menu_item($path, $title, $desc, $menu, $plid, $weight, $module);

    // If there are children do them too.
    if ($children) {
      install_menu_create_menu_items($children, $mlid);
    }
  }
}


/**
 * Disable a specified menu item by mlid.
 *
 * @param $mlid
 *   The mlid of the menu item to disable.
 *
 */
function install_menu_disable_item($mlid, $unhide = FALSE) {
  $link = menu_link_load($mlid);
  $status = ($unhide) ? 0 : 1;
  // If the mlid exists then we can disable it.
  if ($link['mlid']) {
    $link['hidden'] = $status;
    db_query("UPDATE {menu_links} SET hidden = %d WHERE mlid = %d", $status, $mlid);
  }
}

/* --- BLOCKS --- */

/**
 * Creates a new block.
 */
function install_add_block($module, $delta, $theme, $status, $weight, $region, $visibility = 0, $pages = '', $custom = 0, $throttle = 0, $title = '') {
  $res = db_result(db_query("SELECT theme FROM {blocks} where module = '%s' AND delta = '%s' AND theme = '%s' AND status = %d AND weight = %d AND region = '%s' ",$module, $delta, $theme, $status, $weight, $region)); 
  if(!$res)
  db_query("INSERT INTO {blocks} (module, delta, theme, status, weight, region, visibility, pages, custom, throttle, title) 
     VALUES ('%s', '%s', '%s', %d, %d, '%s', %d, '%s', %d, %d, '%s')", 
     $module, $delta, $theme, $status, $weight, $region, $visibility, $pages, $custom, $throttle, $title);
  if ($module == 'block') {
    $box = db_fetch_object(db_query('SELECT * FROM {boxes} WHERE bid=%d', $delta));
    db_query("INSERT INTO {boxes} (bid, body, info, format) VALUES (%d, '%s', '%s', '%s')", $box->bid, $box->body, $box->info, $box->format);
  }
}

/**
 * Creates a new block role.
 */
function install_add_block_role($module, $delta, $rid) {
  db_query("INSERT INTO {blocks_roles} (module,delta,rid) VALUES ('%s', '%s', %d)", $module, $delta, $rid);
}

/* --- TINYMCE --- */

/**
 * Add roles to an existing TinyMCE profile identified by the profile name
 */
/*
function install_tinymce_add_roles($name, $roles) {
  foreach ($roles as $role) {
    db_query("INSERT INTO {tinymce_role} (name, rid) VALUES ('%s', %d)", $name, $role);
  }
}
*/
/**
 * Create a new TinyMCE profile and set the settings
 * @param	$name		A text string identifying the profile
 * @param	$settings	An associative array containing key value pairs
 */
/*
function install_tinymce_create_profile($name, $settings) {
  db_query("INSERT INTO {tinymce_settings} (name, settings) VALUES ('%s', '%s')", $name, serialize($settings));
}
*/
/* --- TAXONOMY --- */

/**
 * Given the name of a vocabulary, return its Vocab ID
 * @param	$name		A text string identifying the vocabulary
 */
function install_get_vid($name) {
  // not guaranteed to be unique, hence the LIMIT
  return db_result(db_query("SELECT vid FROM {vocabulary} WHERE name = '%s' LIMIT 1", $name));
};

/* --- FILTER --- */

/**
 * Set the roles that can be used with the filter 
 * @param	$roles		An array of role IDs
 * @param	$format_id	An integer of the format ID
 */
function install_format_set_roles($roles, $format_id) {
  $roles = implode(',',$roles);	
  db_query("UPDATE {filter_formats} SET roles = '%s' WHERE format = %d", $roles, $format_id);
}

/**
 * Add a new input format 
 * @param	$name	The human-readable name of the new format
 * @param	$cache	If this format is cacheable
 */
function install_add_format($name, $cache = 1) {
  $format_id = db_next_id('{filter_formats}_fid');
  // Check explicitly for format_id <= 3. If the database was improperly prefixed,
  // this would cause a nasty infinite loop or duplicate mlid errors.
  // TODO: have automatic prefixing through an installer to prevent this.
  while ($format_id <= 3) {
    $format_id = db_next_id('{filter_formats}_fid');
  }
  db_query("INSERT INTO {filter_formats} (`format`, `name`, `roles`, `cache`) VALUES (%d, '%s', '', %d)", $format_id, $name, $cache);
  return $format_id;
}

/**
 * Remove the specified filter from the specified format 
 * @param	$format_id	The ID of the format to remove the filter from
 * @param	$module		The module this filter belongs to
 * @param	$delta		The delta of this filter
 *
 * NOTE: the module name + the delta is what uniquely identifies a filter
 */
function install_remove_filter($format_id, $module, $delta) {	
  db_query("DELETE FROM {filters} WHERE format = %d AND module = '%s' AND delta = %d", $format_id, $module, $delta);
}

/**
 * Add a filter to an input format
 * @param	$format_id	The ID of the format to add the filter to
 * @param	$module		The module this filter belongs to
 * @param	$delta		The delta of this filter
 * @param	$weight		The weight to be applied to this filter
 */
function install_add_filter($format_id, $module, $delta = 0, $weight = 0) {
  db_query("INSERT INTO {filters} (`format`, `module`, `delta`, `weight`) VALUES (%d, '%s', %d, %d)", $format_id, $module, $delta, $weight);
}

/* --- THEME --- */

/**
 * Example usage:
 * // Set site theme
 * install_disable_theme("garland");
 * install_default_theme("mytheme");
 */

/**
* Enable theme
* @param	$theme	Unique string that is the name of theme
*/
function install_enable_theme($theme) {
  system_theme_data();
  db_query("UPDATE {system} SET status = 1 WHERE type = 'theme' and name = '%s'", $theme);
  system_initialize_theme_blocks($theme);
}

/**
* Disable theme
* @param	$theme	Unique string that is the name of theme
*/
function install_disable_theme($theme) {
  system_theme_data();
  db_query("UPDATE {system} SET status = 0 WHERE type = 'theme' and name ='%s'", $theme);
}

/**
* Set default theme
* @param	$theme	Unique string that is the name of theme
*/
function install_default_theme($theme) {
  install_enable_theme($theme);
  variable_set('theme_default', $theme);
}

/**
* Set admin theme
* @param	$theme	Unique string that is the name of theme
*/
function install_admin_theme($theme) {
  variable_set('admin_theme', $theme);
}

function menu_save_item(&$item) {
  $existing_item = NULL;

  // Check that the item already exists in the menu tree, if $item['mlid'] is
  // specified.
  if (isset($item['mlid'])) {
    $existing_item = menu_get_item($item['mlid']);
  }

  if ($item['mlid'] && !empty($existing_item)) {
    db_query("UPDATE {menu} SET plid = %d, path = '%s', title = '%s', description = '%s', weight = %d, type = %d WHERE mlid = %d", $item['plid'], $item['path'], $item['title'], $item['description'], $item['weight'], $item['type'], $item['mlid']);
    return SAVED_UPDATED;
  }
  else {
    $item['mlid'] = db_next_id('{menu}_mlid');
    // Check explicitly for mlid <= 2. If the database was improperly prefixed,
    // this would cause a nasty infinite loop or duplicate mlid errors.
    // TODO: have automatic prefixing through an installer to prevent this.
    while ($item['mlid'] <= 2) {
      $item['mlid'] = db_next_id('{menu}_mlid');
    }
    db_query("INSERT INTO {menu} (mlid, plid, path, title, description, weight, type) VALUES (%d, %d, '%s', '%s', '%s', %d, %d)", $item['mlid'], $item['plid'], $item['path'], $item['title'], $item['description'], $item['weight'], $item['type']);
    return SAVED_NEW;
  }
}


function db_next_id($name) {
  $id = db_result(db_query("SELECT nextval('%s_seq')", db_prefix_tables($name)));
  return $id;
}

function install_add_language($l) {
  locale_add_language($l['langcode'], $l['name'], $l['native'], $l['direction'], $l['domain'], $l['prefix'], $l['enabled'], $l['default']);
  // Begin building file object.
  $file = new stdClass();
  $file->filename = file_munge_filename(trim(basename($l['filepath']), '.'), $extensions);
  $file->filepath = $l['filepath'];
  $file->filemime = file_get_mimetype($file->filename);

  // If the destination is not provided, or is not writable, then use the
  // temporary directory.
  if (empty($dest) || file_check_path($dest) === FALSE) {
    $dest = file_directory_temp();
  }

  $file->source = 'file';
  $file->destination = file_destination(file_create_path($dest .'/'. $file->filename), FILE_EXISTS_REPLACE);
  _locale_import_po($file, $l['langcode'], LOCALE_IMPORT_KEEP, 'default');
}